/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_stack.c
 * @brief   X86 stack instruction encoders.
 * @author     Ewerton23929dev
 *
 * @details
 * Implements PUSH and POP for registers and immediates, with REX.B for the
 * extended range and the choice between sign extended imm8 and imm32.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

/*
 * x86_stack.c — PUSH / POP encoders
 *
 *  PUSH Reg  : 50+rd            (REX.B for R8-R15)
 *  PUSH Imm  : 6A ib / 68 id    (imm8 sign-extended vs imm32)
 *  POP  Reg  : 58+rd            (REX.B for R8-R15)
 *  POP  Mem  : 8F /0  (not implemented — via ModRM)
 */

#include "../x86_helpers.h"
#include "x86_instructions.h"

// ---------------------------------------------------------------------------
// PUSH Reg
// ---------------------------------------------------------------------------
bool X86_CaseMountPushReg(X86BackendContext *ctx)
{
    const FusHidrNode_t *mir = ctx->hidr;
    x86Instruction_t *enc    = ctx->encoder;

    if (mir->dst.type != HIDR_OPERAND_TYPE_REG)  return false;
    if (mir->src.type != HIDR_OPERAND_TYPE_NONE) return false;

    size_t reg = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    if (reg == (size_t)-1) return false;

    enc->opcode.opcode[0]   = 0x50 + (reg & 0x7);
    enc->opcode.opcode_size = 1;

    if (reg >= 8) {
        enc->rex.b = 1;
        enc->has_rex = true;
    }
    return true;
}

// ---------------------------------------------------------------------------
// PUSH Imm
//  6A ib for imm8, 68 id for imm32
// ---------------------------------------------------------------------------
bool X86_CaseMountPushImm(X86BackendContext *ctx)
{
    const FusHidrNode_t *mir = ctx->hidr;
    x86Instruction_t *enc    = ctx->encoder;

    if (mir->dst.type != HIDR_OPERAND_TYPE_IMM)  return false;
    if (mir->src.type != HIDR_OPERAND_TYPE_NONE) return false;

    size_t sz = X86_CalMirImmSize(mir->dst.data.imm.size);
    if (!sz) return false;

    // Choose push opcode by imm size
    if (sz == 1) {
        enc->opcode.opcode[0] = 0x6A; // push imm8
    } else {
        enc->opcode.opcode[0] = 0x68; // push imm32
        sz = 4; // force 4 for 16/32 (push imm16 not used in 64-bit)
    }
    enc->opcode.opcode_size = 1;

    enc->imm.value = mir->dst.data.imm.imm;
    enc->imm.size  = sz;
    enc->has_imm   = true;
    return true;
}

// ---------------------------------------------------------------------------
// POP Reg
// ---------------------------------------------------------------------------
bool X86_CaseMountPopReg(X86BackendContext *ctx)
{
    const FusHidrNode_t *mir = ctx->hidr;
    x86Instruction_t *enc    = ctx->encoder;

    if (mir->dst.type != HIDR_OPERAND_TYPE_REG)  return false;
    if (mir->src.type != HIDR_OPERAND_TYPE_NONE) return false;

    size_t reg = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    if (reg == (size_t)-1) return false;

    enc->opcode.opcode[0]   = 0x58 + (reg & 0x7);
    enc->opcode.opcode_size = 1;

    if (reg >= 8) {
        enc->rex.b = 1;
        enc->has_rex = true;
    }
    return true;
}

// ---------------------------------------------------------------------------
// POP — stack pop to memory (via ModRM, not used for Reg)
//  8F /0  — POP r/m
// ---------------------------------------------------------------------------
bool X86_CaseMountPopImm(X86BackendContext *ctx)
{
    const FusHidrNode_t *mir = ctx->hidr;
    x86Instruction_t *enc    = ctx->encoder;

    if (mir->dst.type != HIDR_OPERAND_TYPE_IMM)  return false;
    if (mir->src.type != HIDR_OPERAND_TYPE_NONE) return false;

    // This encoding is for POP r/m64 — used as placeholder for stack adjust
    enc->rex.w = 1;
    enc->has_rex = true;

    enc->opcode.opcode[0]   = 0x83; // SUB RSP, imm8  (stack shrink) — alternative
    enc->opcode.opcode_size = 1;

    enc->modrm.mod = MODRM_MOD_REG_DIRECT;
    enc->modrm.reg = 0;
    enc->modrm.rm  = X86_REG_RSP;
    enc->has_modrm = true;

    enc->imm.value = mir->dst.data.imm.imm;
    enc->imm.size  = 1;
    enc->has_imm   = true;
    return true;
}
