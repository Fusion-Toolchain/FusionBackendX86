/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_types.h
 * @brief   X86 instruction and relocation types.
 * @author     Ewerton23929dev
 *
 * @details
 * Defines the structure representing a mounted instruction and the relocation
 * enumeration (REL32 and ABS64) shared with the linker.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

#ifndef X86_BACKEND_TYPES
#define X86_BACKEND_TYPES

#include <stdint.h>
#include <stdbool.h>

/* ============================================================================
 * Relocation types (used by linker)
 * ========================================================================= */
typedef enum {
    X86_REL32,  // 32-bit PC-relative
    X86_ABS64   // 64-bit absolute
} X86ReallocTypes_t;

/* ============================================================================
 * Physical registers (X86_REG_*) — single source of truth
 * ========================================================================= */
#define X86_REG_RAX  0
#define X86_REG_RCX  1
#define X86_REG_RDX  2
#define X86_REG_RBX  3
#define X86_REG_RSP  4
#define X86_REG_RBP  5
#define X86_REG_RSI  6
#define X86_REG_RDI  7
#define X86_REG_R8   8
#define X86_REG_R9   9
#define X86_REG_R10 10
#define X86_REG_R11 11
#define X86_REG_R12 12
#define X86_REG_R13 13
#define X86_REG_R14 14
#define X86_REG_R15 15

/* ============================================================================
 * ModR/M encoding
 * ========================================================================= */
#define MODRM_MOD_MEM_00         0x0  // [r/m]
#define MODRM_MOD_MEM_8BIT_DISP  0x1  // [r/m + disp8]
#define MODRM_MOD_MEM_32BIT_DISP 0x2  // [r/m + disp32]
#define MODRM_MOD_REG_DIRECT     0x3  // r/m is register

#define MODRM_RM_MAX_VALUE  7
#define MODRM_REG_MAX_VALUE 7
#define MODRM_MOD_MAX_VALUE 3

/* ============================================================================
 * SIB encoding
 * ========================================================================= */
#define X86_SIB_INDEX_NONE 4

#define X86_SIB_SCALE_1 0
#define X86_SIB_SCALE_2 1
#define X86_SIB_SCALE_4 2
#define X86_SIB_SCALE_8 3

/* ============================================================================
 * Instruction fields
 * ========================================================================= */
typedef struct {
    uint8_t opcode[3];
    uint8_t opcode_size;
} x86Opcode_t;

typedef struct {
    uint8_t mod; // 2 bits
    uint8_t reg; // 3 bits
    uint8_t rm;  // 3 bits
} x86ModRm_t;

typedef struct {
    uint8_t prefix[4];
    uint8_t prefix_size;
} x86Prefix_t;

typedef struct {
    uint64_t value;
    uint8_t  size; // 1,2,4,8
} x86Imm_t;

typedef struct {
    uint8_t scale; // 2 bits
    uint8_t index; // 3 bits
    uint8_t base;  // 3 bits
} x86Sib_t;

typedef struct {
    int32_t value;
    uint8_t size; // 1 or 4
} x86Disp_t;

typedef struct {
    uint8_t w : 1; // 64-bit operand
    uint8_t r : 1; // extension of ModRM.reg
    uint8_t x : 1; // extension of SIB.index
    uint8_t b : 1; // extension of ModRM.rm / opcode reg
} x86Rex_t;

/* ============================================================================
 * Complete X86 instruction (prefix + REX + opcode + ModRM/SIB/disp/imm)
 * ========================================================================= */
typedef struct {
    x86Prefix_t prefix;
    x86Rex_t    rex;
    x86Opcode_t opcode;
    x86ModRm_t  modrm;
    x86Imm_t    imm;
    x86Sib_t    sib;
    x86Disp_t   disp;

    bool has_prefix;
    bool has_modrm;
    bool has_imm;
    bool has_sib;
    bool has_disp;
    bool has_rex;
} x86Instruction_t;

#endif
