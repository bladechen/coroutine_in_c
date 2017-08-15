CFLAGS := -Wall -Werror -O0 -ggdb3
LDFLAGS := #-m32

ifeq ($(ARCH),32)
	CFLAGS += -m32
	LDFLAGS += -m32
else
endif


CC := gcc
SRC_FILE := $(wildcard *.c)
OBJ_FILE := $(SRC_FILE:.c=.o)
ASM_SRC_FILE := $(wildcard *.S)
ASM_OBJ_FILE += $(ASM_SRC_FILE:.S=.o)
INCLUDES := -I./
LIBS =
TARGET = coro

all: $(TARGET)

# $(TARGET): coro.o arch_ctx.o
# 	gcc -o coro coro.o arch_ctx.o


all32:
	make -e ARCH=32


-include $(ASM_SRC_FILE:.S=.d)
%.d: %.S
	@set -e; rm -f $@; \
    $(CC) -MM $(CFLAGS)  $< > $@.$$$$; \
    sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
    rm -f $@.$$$$

-include $(SRC_FILE:.c=.d)
%.d: %.c
	@set -e; rm -f $@; \
    $(CC) -MM $(CFLAGS)  $< > $@.$$$$; \
    sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
    rm -f $@.$$$$

$(TARGET): $(OBJ_FILE) $(ASM_OBJ_FILE)
	$(CC) $^ -o $@ $(LIBS) $(LDFLAGS)

.PHONY:clean
clean:
	-rm -f $(OBJ_FILE) $(TARGET) $(ASM_OBJ_FILE) *.d *.o
