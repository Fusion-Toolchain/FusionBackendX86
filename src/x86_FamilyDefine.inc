/*
 * x86_FamilyDefine.inc — HIDR opcode -> builder rule tables
 *
 * Each family maps (dst_type, src_type) to a builder function.
 * Included by x86_interface.c.
 */

// ---------------------------------------------------------------------------
// MOV — 5 variants
// ---------------------------------------------------------------------------
static X86FamilyRule_t mov_rules[] = {
    [0] = {.builder = X86_CaseMountMovImmReg, .dst_type = HIDR_OPERAND_TYPE_REG,     .src_type = HIDR_OPERAND_TYPE_IMM},
    [1] = {.builder = X86_CaseMountMovRegReg, .dst_type = HIDR_OPERAND_TYPE_REG,     .src_type = HIDR_OPERAND_TYPE_REG},
    [2] = {.builder = X86_CaseMountMovMemImm, .dst_type = HIDR_OPERAND_TYPE_MEM_REF, .src_type = HIDR_OPERAND_TYPE_IMM},
    [3] = {.builder = X86_CaseMountMovSymReg, .dst_type = HIDR_OPERAND_TYPE_REG,     .src_type = HIDR_OPERAND_TYPE_SYM},
    [4] = {.builder = X86_CaseMountMovRegMem, .dst_type = HIDR_OPERAND_TYPE_REG,     .src_type = HIDR_OPERAND_TYPE_MEM_REF},
};

// ---------------------------------------------------------------------------
// RET / SYSCALL — no operands
// ---------------------------------------------------------------------------
static X86FamilyRule_t ret_rules[] = {
    [0] = {.builder = X86_MountRet, .dst_type = HIDR_OPERAND_TYPE_NONE, .src_type = HIDR_OPERAND_TYPE_NONE},
};

static X86FamilyRule_t syscall_rules[] = {
    [0] = {.builder = X86_CaseMountSyscall, .dst_type = HIDR_OPERAND_TYPE_NONE, .src_type = HIDR_OPERAND_TYPE_NONE},
};

// ---------------------------------------------------------------------------
// ALU: ADD / CMP
// ---------------------------------------------------------------------------
static X86FamilyRule_t add_rules[] = {
    {.builder = X86_CaseMountAddRegReg, .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_REG},
    {.builder = X86_CaseMountAddImmReg, .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_IMM},
};

static X86FamilyRule_t cmp_rule[] = {
    {.builder = X86_CaseMountCmpRegReg, .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_REG},
    {.builder = X86_CaseMountCmpRegImm, .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_IMM},
};

// ---------------------------------------------------------------------------
// Memory / Control
// ---------------------------------------------------------------------------
static X86FamilyRule_t lea_rules[] = {
    {.builder = X86_CaseMountLeaRegMem, .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_MEM_REF},
};

static X86FamilyRule_t call_rules[] = {
    [0] = {.builder = X86_CaseMountCallReg,   .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_NONE},
    [1] = {.builder = X86_CaseMountCallRel32, .dst_type = HIDR_OPERAND_TYPE_SYM, .src_type = HIDR_OPERAND_TYPE_NONE},
};

static X86FamilyRule_t push_rules[] = {
    [0] = {.builder = X86_CaseMountPushReg, .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_NONE},
    [1] = {.builder = X86_CaseMountPushImm, .dst_type = HIDR_OPERAND_TYPE_IMM, .src_type = HIDR_OPERAND_TYPE_NONE},
};

static X86FamilyRule_t pop_rules[] = {
    [0] = {.builder = X86_CaseMountPopReg, .dst_type = HIDR_OPERAND_TYPE_REG, .src_type = HIDR_OPERAND_TYPE_NONE},
    [1] = {.builder = X86_CaseMountPopImm, .dst_type = HIDR_OPERAND_TYPE_IMM, .src_type = HIDR_OPERAND_TYPE_NONE},
};

// ---------------------------------------------------------------------------
// Family table
// ---------------------------------------------------------------------------
static x86Familys_t familys[] = {
    [0] = {.opcode = HIDR_INSTR_MOV,     .rules = mov_rules,     .rule_count = sizeof(mov_rules)/sizeof(mov_rules[0])},
    [1] = {.opcode = HIDR_INSTR_RET,     .rules = ret_rules,     .rule_count = sizeof(ret_rules)/sizeof(ret_rules[0])},
    [2] = {.opcode = HIDR_INSTR_ADD,     .rules = add_rules,     .rule_count = 2},
    [3] = {.opcode = HIDR_INSTR_ADDR,    .rules = lea_rules,     .rule_count = 1},
    [4] = {.opcode = HIDR_INSTR_CALL,    .rules = call_rules,    .rule_count = 2},
    [5] = {.opcode = HIDR_INSTR_PUSH,    .rules = push_rules,    .rule_count = 2},
    [6] = {.opcode = HIDR_INSTR_POP,     .rules = pop_rules,     .rule_count = 2},
    [7] = {.opcode = HIDR_INSTR_SYSCALL, .rules = syscall_rules, .rule_count = 1},
    [8] = {.opcode = HIDR_INSTR_CMP,     .rules = cmp_rule,      .rule_count = 2},
};
