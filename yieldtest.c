#include <stdio.h>
#include <pthread.h>

#include "yieldtest.h"
// note: yield.h should be last include, if you use threading
#include "yield.h"

int total = 0;

/*void* counter_1(void* unused) {
    int sum1 = 0;
    int data;
    struct c_generator c1 = {0};
    c1.data = &data;
    while(counter(&c1), !c1.end) {
        total += data;
    }
    printf("1sum: %d\n", sum1);
}

void* counter_2(void* unused) {
    int sum2 = 0;
    int data;
    struct c_generator c2 = {0};
    c2.data = &data;
    while(counter(&c2), !c2.end) {
        total += data;
    }
    printf("2sum: %d\n", sum2);
}
*/

int range(int start, int end, struct c_generator* c_gen) {
    yield_function(c_gen);

    int i = start;
    while (i < end) {
        yield(i);
        i++;
    }
}

int n_generator(int n, struct c_generator* c_gen) {
    yield_function(c_gen);
    while(1)
        yield(n);
}



char* test_generator_sumrange(int low, int high, int goal) {
    int sum = 0;
    int i;
    struct c_generator gen = {0};
    gen.data = &i;
    while (range(low, high, &gen), !gen.end) {
        sum += i;
    }
    return TEST_STR(sum == goal);
}

char* test_generator_n(int number, int times) {
    bool passed = true;
    int i;
    struct c_generator gen = {0};
    gen.data = &i;
    while (times--) {
        n_generator(number, &gen);
        if (i != number) passed = false;
    }
    return TEST_STR(passed);
}

char* test_generator_multiple() {
    bool passed = true;
    struct c_generator range_gen = {0};
    struct c_generator n_gen = {0};
    int range_dat, n_dat;
    range_gen.data = &range_dat; n_gen.data = &n_dat;
    range(0, 10, &range_gen);
    printf("rang: %d\n", range_dat);
    if (range_dat != 0) {
        passed = false;
    }
    n_generator(100, &n_gen);
    if (n_dat != 100) {
        passed = false;
    }
    range(0, 10, &range_gen);
    printf("rang: %d\n", range_dat);
    if (range_dat != 1) {
        passed = false;
    }
    range(0, 10, &range_gen);
    printf("rang: %d\n", range_dat);
    if (range_dat != 2) {
        passed = false;
    }
    range(0, 10, &range_gen);
    printf("rang: %d\n", range_dat);
    if (range_dat != 3) {
        passed = false;
    }
    n_generator(100, &n_gen);
    if (n_dat != 100) {
        passed = false;
    }
    n_generator(100, &n_gen);
    if (n_dat != 100) {
        passed = false;
    }
    range(0, 10, &range_gen);
    printf("rang: %d\n", range_dat);
    if (range_dat != 4) {
        passed = false;
    }

    return TEST_STR(passed);
}



int main () {
    printf("Testing yield():\n");
    /*printf("Testing range(0, 5) sum test: %s\n", test_generator_sumrange(0, 5, 10));
    printf("Testing range(0, 1000) sum test: %s\n", test_generator_sumrange(0, 1000, 499500));
    printf("Testing n generator gives 1 1: %s\n", test_generator_n(1, 1));
    printf("Testing n generator gives 5 1s: %s\n", test_generator_n(1, 5));
    printf("Testing n generator gives 10 90s: %s\n", test_generator_n(90, 10));*/
    printf("Testing multiple interleaved generator calls: %s\n", test_generator_multiple());
}