#ifndef X86_SIZE_INSTRUCTION_H
#define X86_SIZE_INSTRUCTION_H
#include "x86_helpers.h"

/* ============================================================================
 * Static sizing (single pass, no encode)
 *
 *  Convention: every helper below is pure (reads only the HIDR node) and
 *  returns 0 when no rule matches. X86_SizeOfHidr converts that 0 into
 *  X86_MAX_INSTR_BYTES, a safe upper bound (slab_size is capacity,
 *  slab_offset is usage - over-alloc is harmless, under-alloc is fatal).
 * ========================================================================= */
#define X86_MAX_INSTR_BYTES 15

/* --- field atoms: "how many bytes does this field take?" --- */

static inline size_t X86_SizePrefix(const FusHidrNode_t *node)
{
    return (node->op_size == HIDR_OP_SIZE_16) ? 1 : 0; // 0x66
}
static inline size_t X86_SizeSib(size_t base)
{
    return ((base & 0x7) == 0x4) ? 1 : 0; // RSP/R12 needs SIB
}

/* disp size for [base + off] under mod 00/01/10 (mod 00 only when off == 0
 * and base is not RBP/R13; disp8 covers 0..127 since offset is uint16_t) */
static inline size_t X86_SizeDispMod010(size_t base, unsigned off)
{
    if (off == 0 && (base & 0x7) != 0x5) return 0;
    if (off <= 0x7F) return 1;
    return 4;
}

static inline int X86_IsExtReg(size_t reg)
{
    return reg >= 8; // R8-R15 (callers reject (size_t)-1 first)
}
static inline size_t X86_SizeRex1(int is64, size_t reg)
{
    return (is64 || X86_IsExtReg(reg)) ? 1 : 0;
}
static inline size_t X86_SizeRex2(int is64, size_t r_reg, size_t b_reg)
{
    return (is64 || X86_IsExtReg(r_reg) || X86_IsExtReg(b_reg)) ? 1 : 0;
}

/* --- operand atoms: mapped values, (size_t)-1 / 0 when invalid --- */

static inline size_t X86_DstReg(const FusHidrNode_t *node)
{
    return X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(node->dst.data.reg));
}
static inline size_t X86_SrcReg(const FusHidrNode_t *node)
{
    return X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(node->src.data.reg));
}
static inline size_t X86_DstBase(const FusHidrNode_t *node)
{
    return X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(node->dst.data.memory_ref.base));
}
static inline size_t X86_SrcBase(const FusHidrNode_t *node)
{
    return X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(node->src.data.memory_ref.base));
}
static inline size_t X86_SrcImmSize(const FusHidrNode_t *node)
{
    return X86_CalMirImmSize(node->src.data.imm.size);
}
static inline size_t X86_DstImmSize(const FusHidrNode_t *node)
{
    return X86_CalMirImmSize(node->dst.data.imm.size);
}

/* --- shape atoms: whole skeletons shared by families --- */

// opcode + ModRM + REX for two regs (r_reg -> ModRM.reg, b_reg -> ModRM.rm)
static inline size_t X86_SizeRegReg(size_t prefix, int is64, size_t r_reg, size_t b_reg)
{
    if (r_reg == (size_t)-1 || b_reg == (size_t)-1) return 0;
    return prefix + 1 + 1 + X86_SizeRex2(is64, r_reg, b_reg);
}
// opcode + ModRM + imm + REX (ADD/CMP reg,imm)
static inline size_t X86_SizeRegImm(size_t prefix, int is64, size_t dst, size_t imm_size)
{
    if (dst == (size_t)-1 || imm_size == 0) return 0;
    return prefix + 1 + 1 + imm_size + X86_SizeRex1(is64, dst);
}
// 50+rd / 58+rd (PUSH/POP reg)
static inline size_t X86_SizePushPopReg(size_t prefix, size_t reg)
{
    if (reg == (size_t)-1) return 0;
    return prefix + 1 + (X86_IsExtReg(reg) ? 1 : 0);
}

/* --- families: one per opcode, 0 = no rule matches --- */

// B8+rd | 89 /r | C7 /0 | 8B /r | B8+rd(sym)
static inline size_t X86_SizeMov(const FusHidrNode_t *node, size_t prefix, int is64)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_IMM) {
        const size_t dst = X86_DstReg(node);
        const size_t imm_size = X86_SrcImmSize(node);
        if (dst == (size_t)-1 || imm_size == 0) return 0;
        const size_t imm = is64
            ? ((node->src.data.imm.size <= HIDR_IMM32) ? 4 : 8)
            : imm_size;
        return prefix + 1 + X86_SizeRex1(is64, dst) + imm;
    }
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_REG)
        return X86_SizeRegReg(prefix, is64, X86_DstReg(node), X86_SrcReg(node));
    if (node->dst.type == HIDR_OPERAND_TYPE_MEM_REF &&
        node->src.type == HIDR_OPERAND_TYPE_IMM) {
        const size_t base = X86_DstBase(node);
        if (base == (size_t)-1) return 0;
        const size_t disp = (node->dst.data.memory_ref.offset <= 127) ? 1 : 4;
        return prefix + 1 + 1 + X86_SizeSib(base) + disp + 4 + X86_SizeRex1(is64, base);
    }
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_MEM_REF) {
        const size_t base = X86_SrcBase(node);
        if (X86_DstReg(node) == (size_t)-1 || base == (size_t)-1) return 0;
        const size_t disp = X86_SizeDispMod010(base, node->src.data.memory_ref.offset);
        return prefix + 1 + 1 + X86_SizeSib(base) + disp + 1; // REX always on
    }
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_SYM) {
        const size_t dst = X86_DstReg(node);
        if (dst == (size_t)-1) return 0;
        return prefix + 1 + (X86_IsExtReg(dst) ? 1 : 0) + 8;
    }
    return 0;
}
// 01 /r (reg=src) | 80/83/81 /0 + imm
static inline size_t X86_SizeAdd(const FusHidrNode_t *node, size_t prefix, int is64)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_REG)
        return X86_SizeRegReg(prefix, is64, X86_SrcReg(node), X86_DstReg(node));
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_IMM)
        return X86_SizeRegImm(prefix, is64, X86_DstReg(node), X86_SrcImmSize(node));
    return 0;
}
// 38/3B /r (reg=dst) | 80/83/81 /7 + imm
static inline size_t X86_SizeCmp(const FusHidrNode_t *node, size_t prefix, int is64)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_REG)
        return X86_SizeRegReg(prefix, is64, X86_DstReg(node), X86_SrcReg(node));
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_IMM)
        return X86_SizeRegImm(prefix, is64, X86_DstReg(node), X86_SrcImmSize(node));
    return 0;
}
// 8D /r + disp, REX always on
static inline size_t X86_SizeLea(const FusHidrNode_t *node, size_t prefix)
{
    if (node->dst.type != HIDR_OPERAND_TYPE_REG ||
        node->src.type != HIDR_OPERAND_TYPE_MEM_REF) return 0;
    const size_t base = X86_SrcBase(node);
    if (X86_DstReg(node) == (size_t)-1 || base == (size_t)-1) return 0;
    if (X86_SizeSib(base)) return 0; // builder rejects SIB base
    return prefix + 1 + 1 + X86_SizeDispMod010(base, node->src.data.memory_ref.offset) + 1;
}
// FF /2 (W never set) | E8 rel32
static inline size_t X86_SizeCall(const FusHidrNode_t *node, size_t prefix)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_NONE) {
        const size_t reg = X86_DstReg(node);
        if (reg == (size_t)-1) return 0;
        return prefix + 1 + 1 + (X86_IsExtReg(reg) ? 1 : 0);
    }
    if (node->dst.type == HIDR_OPERAND_TYPE_SYM &&
        node->src.type == HIDR_OPERAND_TYPE_NONE)
        return prefix + 1 + 4;
    return 0;
}
// 50+rd | 6A ib / 68 id
static inline size_t X86_SizePush(const FusHidrNode_t *node, size_t prefix)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_NONE)
        return X86_SizePushPopReg(prefix, X86_DstReg(node));
    if (node->dst.type == HIDR_OPERAND_TYPE_IMM &&
        node->src.type == HIDR_OPERAND_TYPE_NONE) {
        const size_t sz = X86_DstImmSize(node);
        if (sz == 0) return 0;
        return prefix + 1 + ((sz == 1) ? 1 : 4);
    }
    return 0;
}
// 58+rd | 83 /0 rsp + imm8 placeholder
static inline size_t X86_SizePop(const FusHidrNode_t *node, size_t prefix)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_REG &&
        node->src.type == HIDR_OPERAND_TYPE_NONE)
        return X86_SizePushPopReg(prefix, X86_DstReg(node));
    if (node->dst.type == HIDR_OPERAND_TYPE_IMM &&
        node->src.type == HIDR_OPERAND_TYPE_NONE)
        return prefix + 1 + 1 + 1 + 1; // REX.W + 83 + ModRM + imm8
    return 0;
}
static inline size_t X86_SizeRet(const FusHidrNode_t *node, size_t prefix)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_NONE &&
        node->src.type == HIDR_OPERAND_TYPE_NONE)
        return prefix + 1; // C3
    return 0;
}
static inline size_t X86_SizeSyscall(const FusHidrNode_t *node, size_t prefix)
{
    if (node->dst.type == HIDR_OPERAND_TYPE_NONE &&
        node->src.type == HIDR_OPERAND_TYPE_NONE)
        return prefix + 2; // 0F 05
    return 0;
}

static inline int X86_NeedsReloc(const FusHidrNode_t *node)
{
    return (node->opcode == HIDR_INSTR_MOV && node->src.type == HIDR_OPERAND_TYPE_SYM) ||
           (node->opcode == HIDR_INSTR_CALL && node->dst.type == HIDR_OPERAND_TYPE_SYM);
}

size_t X86_SizeOfHidr(const FusHidrNode_t *node)
{
    if (unlikely(!node)) return X86_MAX_INSTR_BYTES;

    const size_t prefix = X86_SizePrefix(node);
    const int is64 = (node->op_size == HIDR_OP_SIZE_64);
    size_t n = 0;

    switch (node->opcode) {
        case HIDR_INSTR_MOV:     n = X86_SizeMov(node, prefix, is64); break;
        case HIDR_INSTR_ADD:     n = X86_SizeAdd(node, prefix, is64); break;
        case HIDR_INSTR_CMP:     n = X86_SizeCmp(node, prefix, is64); break;
        case HIDR_INSTR_ADDR:    n = X86_SizeLea(node, prefix); break;
        case HIDR_INSTR_CALL:    n = X86_SizeCall(node, prefix); break;
        case HIDR_INSTR_PUSH:    n = X86_SizePush(node, prefix); break;
        case HIDR_INSTR_POP:     n = X86_SizePop(node, prefix); break;
        case HIDR_INSTR_RET:     n = X86_SizeRet(node, prefix); break;
        case HIDR_INSTR_SYSCALL: n = X86_SizeSyscall(node, prefix); break;
        default: break;
    }

    return n ? n : X86_MAX_INSTR_BYTES;
}
#endif