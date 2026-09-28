/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_helpers.h
 * @brief   X86 encoding helpers.
 * @author     Ewerton23929dev
 *
 * @details
 * Centralizes virtual to physical register mapping, immediate size selection and
 * the assembly of ModRM, SIB and REX.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

#ifndef X86_INTERNAL_HELPERS_H
#define X86_INTERNAL_HELPERS_H

#include <Fusion/IRTypes/HidrType.h>
#include <Internal/IRTypes/Fus_HidrRegistre.h>
#include <Internal/Backend/Fus_Backend.h>
#include <Internal/Helpers/Fus_Helper_Codebase.h>

#include <stddef.h>
#include "x86_types.h"

/* ============================================================================
 * Immediate size helper
 * ========================================================================= */
static inline size_t X86_CalMirImmSize(FusHidrImmSize_t size_enum)
{
    switch (size_enum) {
        case HIDR_IMM8:  return 1;
        case HIDR_IMM16: return 2;
        case HIDR_IMM32: return 4;
        case HIDR_IMM64: return 8;
        default:         return 0;
    }
}

/* ============================================================================
 * HIDR register -> X86 physical register
 *
 *  GP singletons (ACC/BASE/COUNTER/DATA/SP/BP/SRC/DST) require index==0
 *  ARG/TMP map to R8-R15 (index 0-7)
 *  SIMD -> xmm0-15, SEG -> 0-5
 *
 *  Returns X86_REG_* or (size_t)-1 if invalid.
 * ========================================================================= */
static inline size_t X86_MapVirtualReg(const HidrRegistre reg)
{
    if (reg.raw == HIDR_REGISTRE_INVALID) return (size_t)-1;

    uint8_t group = reg.desc.group;
    uint8_t role  = reg.desc.role;
    uint8_t index = reg.desc.index;

    if (group == HIDR_REGISTRE_GROUP_GP) {
        switch (role) {
            case HIDR_REGISTRE_ROLE_ACC:
            case HIDR_REGISTRE_ROLE_COUNTER:
            case HIDR_REGISTRE_ROLE_DATA:
            case HIDR_REGISTRE_ROLE_BASE:
            case HIDR_REGISTRE_ROLE_SP:
            case HIDR_REGISTRE_ROLE_BP:
            case HIDR_REGISTRE_ROLE_SRC:
            case HIDR_REGISTRE_ROLE_DST:
                if (index != 0) return (size_t)-1;
                if (role == HIDR_REGISTRE_ROLE_ACC)     return X86_REG_RAX;
                if (role == HIDR_REGISTRE_ROLE_COUNTER) return X86_REG_RCX;
                if (role == HIDR_REGISTRE_ROLE_DATA)    return X86_REG_RDX;
                if (role == HIDR_REGISTRE_ROLE_BASE)    return X86_REG_RBX;
                if (role == HIDR_REGISTRE_ROLE_SP)      return X86_REG_RSP;
                if (role == HIDR_REGISTRE_ROLE_BP)      return X86_REG_RBP;
                if (role == HIDR_REGISTRE_ROLE_SRC)     return X86_REG_RSI;
                return X86_REG_RDI;
            case HIDR_REGISTRE_ROLE_ARG:
            case HIDR_REGISTRE_ROLE_TMP:
                if (index < 8) return X86_REG_R8 + index;
                return (size_t)-1;
            default:
                return (size_t)-1;
        }
    }

    if (group == HIDR_REGISTRE_GROUP_SIMD) {
        if (index < 16) return index; // xmm0-xmm15
        return (size_t)-1;
    }

    if (group == HIDR_REGISTRE_GROUP_SEG) {
        if (index < 6) return index;
        return (size_t)-1;
    }

    return (size_t)-1;
}

/* ============================================================================
 * Common REX helpers (used by InstructionSets to avoid duplication)
 * ========================================================================= */
static inline void X86_SetRexW(x86Instruction_t *instr, bool is64)
{
    instr->rex.w = is64 ? 1 : 0;
    if (is64) instr->has_rex = true;
}

#endif
