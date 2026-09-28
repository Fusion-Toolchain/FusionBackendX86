/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_mov.c
 * @brief   X86 MOV family encoders.
 * @author     Ewerton23929dev
 *
 * @details
 * Covers the five HIDR variants (immediate, register, memory and symbol to
 * register). The symbol case emits an absolute value with a 64 bit
 * relocation for the linker to patch.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

/*
 * x86_mov.c — MOV family encoders
 *
 * Covers 5 HIDR variants:
 *  - MOV Imm -> Reg   (B8+rd)
 *  - MOV Reg -> Reg   (89 /r)
 *  - MOV Reg -> Mem   (8B /r)
 *  - MOV Mem <- Imm   (C7 /0)
 *  - MOV Sym -> Reg   (B8+rd + ABS64 reloc)
 */

#include "../x86_helpers.h"
#include "x86_instructions.h"

#include <BackendInterface/Backend.h>
#include <stdbool.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// MOV Imm -> Reg  (B8+rd)
//  REX.W for 64-bit, REX.B for R8-R15, opcode = B8 + (reg & 7)
// ---------------------------------------------------------------------------
bool X86_CaseMountMovImmReg(X86BackendContext *backend_ctx)
{
    const FusHidrNode_t *mir = backend_ctx->hidr;
    x86Instruction_t *enc    = backend_ctx->encoder;

    FusHidrImmSize_t imm_type = mir->src.data.imm.size;
    size_t imm_size = X86_CalMirImmSize(imm_type);
    if (!imm_size) return false;

    size_t dst = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    if (dst == (size_t)-1) return false;

    // Opcode
    enc->opcode.opcode[0]   = 0xB8 + (dst & 0x7);
    enc->opcode.opcode_size = 1;

    // REX: W for 64-bit, B for extended reg
    enc->has_rex = false;
    if (mir->op_size == HIDR_OP_SIZE_64) {
        enc->rex.w = 1;
        enc->has_rex = true;
        if (dst >= 8) enc->rex.b = 1;
        enc->imm.size = (imm_type <= HIDR_IMM32) ? 4 : 8; // imm32 sign-extended or imm64
    } else {
        if (dst >= 8) {
            enc->rex.b = 1;
            enc->has_rex = true;
        }
        enc->imm.size = imm_size;
    }

    enc->imm.value = mir->src.data.imm.imm;
    enc->has_imm = true;
    return true;
}

// ---------------------------------------------------------------------------
// MOV Reg -> Reg  (89 /r  — MOV r/m, r)
//  reg = dst, rm = src  (dst = dst op, src = src op)
// ---------------------------------------------------------------------------
bool X86_CaseMountMovRegReg(X86BackendContext *backend_ctx)
{
    const FusHidrNode_t *mir = backend_ctx->hidr;
    x86Instruction_t *enc    = backend_ctx->encoder;

    size_t dst = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    size_t src = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->src.data.reg));
    if (dst == (size_t)-1 || src == (size_t)-1) return false;

    enc->opcode.opcode[0]   = 0x89;
    enc->opcode.opcode_size = 1;

    enc->modrm.mod = MODRM_MOD_REG_DIRECT;
    enc->modrm.reg = dst & 0x7;
    enc->modrm.rm  = src & 0x7;
    enc->has_modrm = true;

    // REX for 64-bit + extended regs
    enc->rex.w = (mir->op_size == HIDR_OP_SIZE_64) ? 1 : 0;
    enc->rex.r = (dst >= 8);
    enc->rex.b = (src >= 8);
    enc->has_rex = (enc->rex.w || enc->rex.r || enc->rex.b);

    return true;
}

// ---------------------------------------------------------------------------
// MOV Mem <- Imm  (C7 /0  — MOV r/m, imm32)
//  r/m = [base + disp], reg = 0
// ---------------------------------------------------------------------------
bool X86_CaseMountMovMemImm(X86BackendContext *backend_ctx)
{
    const FusHidrNode_t *mir = backend_ctx->hidr;
    x86Instruction_t *enc    = backend_ctx->encoder;

    size_t base = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.memory_ref.base));
    if (base == (size_t)-1) return false;

    int32_t off = mir->dst.data.memory_ref.offset;

    enc->opcode.opcode[0]   = 0xC7;
    enc->opcode.opcode_size = 1;
    enc->modrm.reg = 0; // /0

    // Disp size -> mod
    if (off >= -128 && off <= 127) {
        enc->disp.size = 1;
        enc->modrm.mod = 1;
    } else {
        enc->disp.size = 4;
        enc->modrm.mod = 2;
    }
    enc->disp.value = off;
    enc->has_disp = true;

    // SIB for RSP/R12 (rm=100)
    if ((base & 0x7) == 4) {
        enc->modrm.rm = 4;
        enc->has_sib = true;
        enc->sib.scale = X86_SIB_SCALE_1;
        enc->sib.index = X86_SIB_INDEX_NONE;
        enc->sib.base  = 4;
    } else {
        enc->modrm.rm = base & 0x7;
        enc->has_sib = false;
    }
    enc->has_modrm = true;

    // REX
    enc->rex.w = (mir->op_size == HIDR_OP_SIZE_64) ? 1 : 0;
    enc->rex.b = (base >= 8);
    enc->has_rex = (enc->rex.w || enc->rex.b);

    enc->imm.value = mir->src.data.imm.imm;
    enc->imm.size  = 4;
    enc->has_imm   = true;
    return true;
}

// ---------------------------------------------------------------------------
// MOV Reg <- Mem  (8B /r  — MOV r, r/m)
//  reg = dst, rm = base
// ---------------------------------------------------------------------------
bool X86_CaseMountMovRegMem(X86BackendContext *backend_ctx)
{
    const FusHidrNode_t *mir = backend_ctx->hidr;
    x86Instruction_t *enc    = backend_ctx->encoder;

    if (mir->dst.type != HIDR_OPERAND_TYPE_REG) return false;
    if (mir->src.type != HIDR_OPERAND_TYPE_MEM_REF) return false;

    size_t dst  = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    size_t base = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->src.data.memory_ref.base));
    if (dst == (size_t)-1 || base == (size_t)-1) return false;

    int32_t off = mir->src.data.memory_ref.offset;

    enc->opcode.opcode[0]   = 0x8B;
    enc->opcode.opcode_size = 1;

    // Mod for disp
    uint8_t mod;
    if (off == 0 && (base & 0x7) != 5) mod = 0;
    else if (off >= -128 && off <= 127) mod = 1;
    else                                mod = 2;

    enc->modrm.mod = mod;
    enc->modrm.reg = dst  & 0x7;
    enc->modrm.rm  = base & 0x7;
    enc->has_modrm = true;

    enc->rex.w = 1;
    enc->rex.r = (dst  >= 8);
    enc->rex.b = (base >= 8);
    enc->has_rex = true;

    if (mod == 1) {
        enc->disp.value = off;
        enc->disp.size  = 1;
        enc->has_disp   = true;
    } else if (mod == 2) {
        enc->disp.value = off;
        enc->disp.size  = 4;
        enc->has_disp   = true;
    }
    return true;
}

// ---------------------------------------------------------------------------
// MOV Sym -> Reg  (B8+rd + ABS64 reloc)
// ---------------------------------------------------------------------------
bool X86_CaseMountMovSymReg(X86BackendContext *backend_ctx)
{
    const FusHidrNode_t *mir = backend_ctx->hidr;
    x86Instruction_t *enc    = backend_ctx->encoder;
    const char *sym = mir->src.data.sym.name;

    size_t dst = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    if (dst == (size_t)-1) return false;

    enc->opcode.opcode[0]   = 0xB8 + (dst & 0x7);
    enc->opcode.opcode_size = 1;

    if (dst >= 8) {
        enc->rex.b = 1;
        enc->has_rex = true;
    }

    enc->imm.value = 0xFFFFFFFFFFFFFFFF;
    enc->imm.size  = 8;
    enc->has_imm   = true;

    size_t off = backend_ctx->block->slab_offset + 2; // points to imm
    FUSB_REGISTRE_REALOCATION(backend_ctx->Api, backend_ctx->block, sym, X86_ABS64, off);
    return true;
}
