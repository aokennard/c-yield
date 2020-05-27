#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "includes/dictionary.h"

// TODO: currently, I dont think it can handle multiple layers of generators as the stack isnt saved with setjmp - possibly use setcontext?
// Additionally, need to include arguments to distinguish calls: range(0, 10) and range(0, 4) are seen as the same...
// plan - take top of stack at function start (yield_function macro), pass all bytes from [ebp + 8] to esp to a hash func, include in pid+funcname hash to get unique

#if defined(_PTHREAD_H)
#define USING_THREADS 1
#define DEFAULT_YIELD_COUNT 8
#else
#define USING_THREADS 0
#endif

#define GENERATOR_END 0x12341234

struct c_generator {
    void** data;
    int end;
};

struct yield_state {
    jmp_buf buf;
    bool yielded;
    bool at_function_end;
#if USING_THREADS
    char funcname[128];
    pthread_t id;
#endif
};

struct yield_state_help {
    struct yield_state* state_data;
    struct c_generator* cgen;
};

#if USING_THREADS

void* yieldstate_copy(void* state) {
    struct yield_state* ys = state;
    struct yield_state* copy = malloc(sizeof(*copy));
    memcpy(copy, ys, sizeof(*ys));
    return copy;
}

void yieldstate_destroy(void* state) {
    struct yield_state* ys = state;
    free(ys);
}

char* pid_funcname_build(pthread_t id, const char* funcname) {
    char* out = malloc(strlen(funcname) + 1 + 50);
    snprintf(out, strlen(funcname) + 50, "%ld_%s", id, funcname);
    return out;
}

void pthread_mutex_unlock_help(pthread_mutex_t** lock) {
    pthread_mutex_unlock(*lock);
}

void free_help(void** data) {
    free(*data);
}

struct state_manager {
    dictionary* yieldstate_dict; // pid_funcname => yieldstate
    pthread_mutex_t state_lock;
};

struct state_manager yield_manager;

void __attribute__((constructor)) state_manager_init()  { 
    yield_manager.yieldstate_dict = dictionary_create(string_hash_function, string_compare, string_copy_constructor, string_destructor, NULL, yieldstate_destroy); 
    pthread_mutex_init(&yield_manager.state_lock, NULL);
}

void __attribute__((destructor)) state_manager_destroy()  {
    dictionary_destroy(yield_manager.yieldstate_dict);
    pthread_mutex_destroy(&yield_manager.state_lock);
}

bool yield_thread_active(const char* funcname) {
    __attribute__ ((cleanup(pthread_mutex_unlock_help))) pthread_mutex_t* copy = &yield_manager.state_lock;
    pthread_mutex_lock(copy);
    __attribute__ ((cleanup(free_help))) char* pid_tuple = pid_funcname_build(pthread_self(), funcname);
    return dictionary_contains(yield_manager.yieldstate_dict, pid_tuple);
}

void yield_thread_fill(const char* funcname) {
    
    __attribute__ ((cleanup(pthread_mutex_unlock_help))) pthread_mutex_t* copy = &yield_manager.state_lock;
    pthread_mutex_lock(copy);

    struct yield_state* pidstate = calloc(1, sizeof(*pidstate));
    pidstate->id = pthread_self();
    __attribute__ ((cleanup(free_help))) char* pid_tuple = pid_funcname_build(pthread_self(), funcname);
    strncpy(pidstate->funcname, funcname, strlen(funcname));

    dictionary_set(yield_manager.yieldstate_dict, pid_tuple, pidstate);
}

void yield_thread_empty(const char* funcname) {
    __attribute__ ((cleanup(free_help))) char* pid_tuple = pid_funcname_build(pthread_self(), funcname);
    if (dictionary_contains(yield_manager.yieldstate_dict, pid_tuple)) {
        dictionary_remove(yield_manager.yieldstate_dict, pid_tuple);
    }
}

#endif

void set_generator_end(struct c_generator* cgen) {
    // make not just x86
    //asm volatile("movl %0, %%eax"::"g"(GENERATOR_END));
    cgen->end = 1;
}

void state_cleanup(struct yield_state_help* state_help) {
    struct yield_state* state = state_help->state_data;

    if (state->at_function_end) {
        memset(&state->buf, 0, sizeof(state->buf));
        state->at_function_end = false;
        set_generator_end(state_help->cgen);
#if USING_THREADS
        yield_thread_empty(__func__);
#endif
    } else {
        state->at_function_end = true;
    }
}

#if USING_THREADS
#define yield_function(gen_type) struct c_generator* __yielder_cgen = gen_type; \
                        bool thread_func_active = yield_thread_active(__func__); \
                        if (!thread_func_active) { yield_thread_fill(__func__); } \
                        __attribute__ ((cleanup(free_help))) char* pid_tuple = pid_funcname_build(pthread_self(), __func__); \
                        struct yield_state* state = dictionary_get(yield_manager.yieldstate_dict, pid_tuple); \
                        __attribute__ ((cleanup(state_cleanup))) struct yield_state_help state_help; \
                        state_help.state_data = state; \
                        state_help.cgen = __yielder_cgen; \
                        if (dictionary_contains(yield_manager.yieldstate_dict, pid_tuple) && ((struct yield_state*)dictionary_get(yield_manager.yieldstate_dict, pid_tuple))->yielded) { \
                            longjmp(((struct yield_state*)dictionary_get(yield_manager.yieldstate_dict, pid_tuple))->buf, 1); } else {}

#else
#define yield_function (gen_type) struct c_generator* __yielder_cgen = gen_type; \
    __attribute__ ((cleanup(state_cleanup))) struct yield_state_help state_help; \
    static struct yield_state state; \
    state_help.state_data = &state; \
    state_help.cgen = __yielder_cgen; \
    if (state.yielded) longjmp(state.buf, 1); else {}
#endif

#if USING_THREADS
#define yield(x) if (setjmp(state->buf)) { state->yielded = false; } \
                 else { state->yielded = true; state->at_function_end = false; *(typeof(x)*)__yielder_cgen->data = x; return x; }
#else
#define yield(x) if (setjmp(state.buf)) { state.yielded = false; } \
    else { state.yielded = true; state.at_function_end = false; *(typeof(x)*)(__yielder_cgen->data) = x; return x; }
#endif

// Multiargs?
// foreach(i, range(5, c_gen), {});
#define foreach(elem, iterable, __gen, callback) \
    do { \
        typeof(elem) _iterdata; \
        __gen.data = &_iterdata; \
        while(iterable, !__gen.end) { \
            { elem = _iterdata; callback; } \
        } \
    } while (0);
        