CC      = gcc
CFLAGS  = -O2 -Wall -fPIC -Wextra -I../include
SRC_DIR = src
OUT_BACKEND = fusbackendX86.a
EXAMPLES_SRC = exemples

BACKEND_SRC := \
	$(SRC_DIR)/x86_pipeline.c \
	$(SRC_DIR)/x86_interface.c \
	$(SRC_DIR)/InstructionSets/x86_mov.c \
	$(SRC_DIR)/InstructionSets/x86_add.c \
	$(SRC_DIR)/InstructionSets/x86_call.c \
	$(SRC_DIR)/InstructionSets/x86_ret.c \
	$(SRC_DIR)/InstructionSets/x86_lea.c \
	$(SRC_DIR)/InstructionSets/x86_stack.c \
	$(SRC_DIR)/InstructionSets/x86_cmp.c \
	$(SRC_DIR)/InstructionSets/x86_syscall.c

BACKEND_OBJ := $(BACKEND_SRC:.c=.o)

$(OUT_BACKEND): $(BACKEND_OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(BACKEND_OBJ) $(OUT_BACKEND)

example:
	$(MAKE) -C $(CORE_PATCH) STATICS_BACKENDS=$(shell pwd)/$(OUT_BACKEND)
	gcc -I$(CORE_PATCH)/include $(EXAMPLES_SRC)/golden_encodingx86.c -L$(CORE_PATCH)/ -lfusion -o golden_encoding

.PHONY: clean