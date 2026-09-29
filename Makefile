FUSION_INCLUDE_CONFIG := fusion-include
NEEDS_CORE := $(filter-out clean,$(or $(MAKECMDGOALS),all))

ifneq ($(NEEDS_CORE),)
  ifeq ($(wildcard $(FUSION_INCLUDE_CONFIG)/modules_flags.mk),)
    $(error Nao achei $(FUSION_INCLUDE_CONFIG)/modules_flags.mk. Use: make CORE_PATH=/caminho/do/Fusion)
  endif
  include $(FUSION_INCLUDE_CONFIG)/modules_flags.mk
endif

CC      = gcc
CFLAGS  = -O2 -Wall $(FUSION_MODULE_FLAG) -Wextra -I$(FUSION_INCLUDE_CONFIG)
SRC_DIR = src
OUT_BACKEND = fusbackendX86.a
EXAMPLES_SRC = exemples

BACKEND_SRC := \
	$(SRC_DIR)/x86_pipeline.c \
	$(SRC_DIR)/x86_interface.c \
	$(SRC_DIR)/x86_linker.c \
	$(SRC_DIR)/x86_hidrmount.c \
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
	rm -rf $(FUSION_INCLUDE_CONFIG)
	rm -f $(BACKEND_OBJ) $(OUT_BACKEND)

example: $(OUT_BACKEND)
	$(MAKE) -C $(CORE_PATH) STATICS_BACKENDS=$(CURDIR)/$(OUT_BACKEND)
	gcc -I$(CORE_PATH)/include $(EXAMPLES_SRC)/golden_encodingx86.c -L$(CORE_PATH)/ -lfusion -Wl,-rpath,$(abspath $(CORE_PATH)) -o golden_encoding

.PHONY: clean example