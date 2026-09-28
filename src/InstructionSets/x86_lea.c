/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_lea.c
 * @brief   X86 LEA instruction encoder.
 * @author     Ewerton23929dev
 *
 * @details
 * Computes the effective address as base plus displacement, emitting 8D /r with
 * mod, SIB, REX.W prefix and an 8, 32 or 64 bit displacement.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

/*
 * x86_lea.c — LEA encoder
 *
 *  LEA r, [base + disp]
 *   opcode 8D /r, mod = 00/01/10, REX.W + REX.R/B
 */

#include "../x86_helpers.h"
#include "../x86_types.h"
#include "x86_instructions.h"

bool X86_CaseMountLeaRegMem(X86BackendContext *ctx)
{
    const FusHidrNode_t *mir = ctx->hidr;
    x86Instruction_t *enc    = ctx->encoder;

    // Validate operands
    if (mir->dst.type != HIDR_OPERAND_TYPE_REG)   return false;
    if (mir->src.type != HIDR_OPERAND_TYPE_MEM_REF) return false;

    size_t dst  = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
    size_t base = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->src.data.memory_ref.base));
    if (dst == (size_t)-1 || base == (size_t)-1) return false;

    uint16_t off = mir->src.data.memory_ref.offset;

    // RSP/R12 as base requires SIB — not supported for LEA here
    if ((base & 0x7) == 0x4) return false; // rm=100 needs SIB

    // Opcode
    enc->opcode.opcode[0]   = 0x8D;
    enc->opcode.opcode_size = 1;

    // ModRM.mod based on disp size and base
    //  RBP/R13 with mod=00 is RIP-relative, so force disp8=0
    uint8_t mod;
    if (off == 0 && (base & 0x7) != 0x5) mod = MODRM_MOD_MEM_00;
    else if (off <= 0x7F)               mod = MODRM_MOD_MEM_8BIT_DISP;
    else                                mod = MODRM_MOD_MEM_32BIT_DISP;

    enc->modrm.mod = mod;
    enc->modrm.reg = dst  & 0x7;
    enc->modrm.rm  = base & 0x7;
    enc->has_modrm = true;

    // REX
    enc->rex.w = 1;
    enc->rex.r = (dst  > 7);
    enc->rex.b = (base > 7);
    enc->has_rex = true;

    // Disp
    if (mod == MODRM_MOD_MEM_8BIT_DISP) {
        enc->disp.value = (int32_t)off;
        enc->disp.size  = 1;
        enc->has_disp   = true;
    } else if (mod == MODRM_MOD_MEM_32BIT_DISP) {
        enc->disp.value = (int32_t)off;
        enc->disp.size  = 4;
        enc->has_disp   = true;
    }
    return true;
}
