
INC=-I./includes/
LDFLAGS = -Llibs/ -lprovided -lpthread
TESTFLAGS = -Wall
CFLAGS = $(INC) -c -g 

OBJS_DIR = objs
YIELD_TEST = yieldtest.o

EXES = yielder

all: $(EXES)

yielder: $(YIELD_TEST:%.o=$(OBJS_DIR)/%.o)
	gcc $^ $(LDFLAGS) -o $@ 

$(OBJS_DIR)/%.o: %.c
	@mkdir -p $(OBJS_DIR)
	gcc $(CFLAGS) $< -o $@

clean:
	rm -rf *.o $(OBJS_DIR) $(EXES)
