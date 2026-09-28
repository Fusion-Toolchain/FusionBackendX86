/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_call.c
 * @brief   X86 CALL instruction encoders.
 * @author     Ewerton23929dev
 *
 * @details
 * Covers indirect call through a register (FF /2) and relative call (E8 rel32),
 * the latter recording a pending PC relative relocation.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

/*
 * x86_call.c — CALL encoders
 *
 *  CALL Reg : FF /2  (r/m64)
 *  CALL Rel : E8 rel32
 */

#include "../x86_helpers.h"
#include "x86_instructions.h"
#include <stdbool.h>

// ---------------------------------------------------------------------------
// CALL Reg  (FF /2)
// ---------------------------------------------------------------------------
bool X86_CaseMountCallReg(X86BackendContext *ctx)
{
    const FusHidrNode_t *mir = ctx->hidr;
    x86Instruction_t *enc    = ctx->encoder;

    if (mir->dst.type != HIDR_OPERAND_TYPE_REG) return false;

    size_t reg = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    if (reg == (size_t)-1) return false;

    enc->opcode.opcode[0]   = 0xFF;
    enc->opcode.opcode_size = 1;

    enc->modrm.mod = MODRM_MOD_REG_DIRECT; // 11
    enc->modrm.reg = 2;                    // /2 = CALL
    enc->modrm.rm  = reg & 0x7;
    enc->has_modrm = true;

    // REX.B for R8-R15, W not needed (FF /2 is 64-bit in long mode)
    enc->rex.w = 0;
    enc->rex.b = (reg >= 8);
    enc->has_rex = (reg >= 8);

    return true;
}

// ---------------------------------------------------------------------------
// CALL Rel32  (E8 rel32)
// ---------------------------------------------------------------------------
bool X86_CaseMountCallRel32(X86BackendContext *ctx)
{
    x86Instruction_t *enc = ctx->encoder;

    enc->opcode.opcode[0]   = 0xE8;
    enc->opcode.opcode_size = 1;

    enc->imm.value = 0; // patched by linker (REL32)
    enc->imm.size  = 4;
    enc->has_imm   = true;
    return true;
}
