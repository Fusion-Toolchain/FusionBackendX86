/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_interface.c
 * @brief   Entry point of the X86 backend.
 * @author     Ewerton23929dev
 *
 * @details
 * Mounts prefixes and REX according to the operation size, selects the family
 * rule from opcode and operands, encodes the bytes and manages the code block
 * lifetime along with the relocations sent to the linker.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

/*
 * x86_interface.c — X86 backend entry point
 *
 *  - Mounts prefix/REX per HIDR op_size
 *  - Selects family rule by opcode + operand types
 *  - Encodes via X86_MountCodeBytes
 *  - Manages code block lifetime and linker relocations
 */

#include <Fusion/IRTypes/HidrType.h>
#include <Fusion/FusionTypes.h>
#include <Internal/Backend/Fus_Backend.h>
#include <Internal/Memory/Fus_Arena.h>
#include <Fusion/Fusion.h>
#include <Internal/Fus_TraceTree.h>
#include <BackendInterface/Backend.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "x86_familys.h"
#include "x86_functions.h"
#include "x86_types.h"
#include "x86_helpers.h"
#include "InstructionSets/x86_instructions.h"
#include "x86_FamilyDefine.inc"

/* ============================================================================
 * Prefix / REX mount (per HIDR)
 * ========================================================================= */
static inline void X86_MountPrefixHidr(const FusHidrNode_t *mir, x86Instruction_t *instr)
{
    instr->has_prefix = false;
    instr->prefix.prefix_size = 0;

    // 0x66 for 16-bit operand size
    if (mir->op_size == HIDR_OP_SIZE_16) {
        instr->prefix.prefix[instr->prefix.prefix_size++] = 0x66;
        instr->has_prefix = true;
    }
}

static inline void X86_MountRex(const FusHidrNode_t *mir, x86Instruction_t *instr)
{
    instr->has_rex = false;

    // W: 64-bit operation (except imm8 special case)
    if (mir->op_size == HIDR_OP_SIZE_64 &&
        !(mir->src.type == HIDR_OPERAND_TYPE_IMM &&
          mir->src.data.imm.size == HIDR_IMM8)) {
        instr->rex.w = 1;
        instr->has_rex = true;
    }

    // R: dst is reg and extended (R8-R15)
    if (mir->dst.type == HIDR_OPERAND_TYPE_REG) {
        size_t idx = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->dst.data.reg));
        if (idx != (size_t)-1 && idx >= 8) {
            instr->rex.r = 1;
            instr->has_rex = true;
        }
    }

    // B: src is reg and extended
    if (mir->src.type == HIDR_OPERAND_TYPE_REG) {
        size_t idx = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->src.data.reg));
        if (idx != (size_t)-1 && idx >= 8) {
            instr->rex.b = 1;
            instr->has_rex = true;
        }
    }

    // B: src is mem base and extended
    if (mir->src.type == HIDR_OPERAND_TYPE_MEM_REF) {
        size_t idx = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir->src.data.memory_ref.base));
        if (idx != (size_t)-1 && idx >= 8) {
            instr->rex.b = 1;
            instr->has_rex = true;
        }
    }
}

/* ============================================================================
 * Family selection
 * ========================================================================= */
static inline bool X86_SelectFamily(FusBackendApi_t *api, X86BackendContext *ctx)
{
    FusTraceTree trace = NULL;
    FUSB_GET_TRACE_FUSION(api, &trace);
    char buf[256];

    const FusHidrNode_t *mir = ctx->hidr;

    for (size_t i = 0; i < sizeof(familys) / sizeof(familys[0]); i++) {
        if (familys[i].opcode != mir->opcode) continue;

        for (size_t j = 0; j < familys[i].rule_count; j++) {
            X86FamilyRule_t *rule = &familys[i].rules[j];
            if (rule->dst_type != mir->dst.type) continue;
            if (rule->src_type != mir->src.type) continue;
            if (rule->builder(ctx)) {
                snprintf(buf, sizeof(buf), "Backend HIDR Family Lookup, Opcode=%u", mir->opcode);
                FUS_PUSH_ERR(trace, FUSION_OK, buf);
                return true;
            }
        }
        goto fail;
    }

fail:
    snprintf(buf, sizeof(buf),
             "Backend Select Family Fail, Opcode=%u Dst=%d,Src=%d",
             mir->opcode, mir->dst.type, mir->src.type);
    FUS_PUSH_ERR(trace, FUSION_ERRO, buf);
    return false;
}

/* ============================================================================
 * Single HIDR -> bytes
 * ========================================================================= */
static FusStatusFlag_t X86_ProcessOnceHidr(
    FusBackendApi_t *api, FusBackendGenerateDataBlock_t *block, const FusHidrNode_t *elem
)
{
    FusTraceTree trace = NULL;
    if (unlikely(!block || !elem)) return FUSION_ERRO;
    FUSB_GET_TRACE_FUSION(api, &trace);

    x86Instruction_t out = {0};
    X86_MountPrefixHidr(elem, &out);

    X86BackendContext ctx = {
        .encoder = &out,
        .hidr    = elem,
        .block   = block,
        .Api     = api,
    };

    X86_MountRex(elem, &out);

    if (unlikely(!X86_SelectFamily(api, &ctx))) return FUSION_ERRO;

    if (unlikely(!X86_MountCodeBytes(&out, &block->slab_offset, block->buffer_slab, block->slab_size))) {
        FUS_PUSH_ERR(trace, FUSION_ERRO, "Backend Process Hidr Step-Fail");
        return FUSION_ERRO;
    }

    FUS_PUSH_ERR(trace, FUSION_OK, "Backend Process Hidr Step-Success");
    return FUSION_OK;
}

/* ============================================================================
 * Block sizing
 * ========================================================================= */
#define X86_MAX_INSTR_BYTES 15
#define X86_MIN_INSTR_BYTES 6

static inline size_t X86DraticCase(const FusHidrNode_t *node)
{
    return (node->op_size == HIDR_OP_SIZE_64) ? X86_MAX_INSTR_BYTES : X86_MIN_INSTR_BYTES;
}

/* ============================================================================
 * Lifetime & mount array
 * ========================================================================= */
static void DestroyLifetimeBlock(const void *data)
{
    FusBackendGenerateDataBlock_t *block = (FusBackendGenerateDataBlock_t*)data;
    FUSB_DESTROY_BLOCK(block->api, block);
}

static FusBackendTransferLifetime_t* X86_BackendMountHidrArry(
    FusBackendApi_t *api, const FusHidrNode_t *hidr, const size_t count
)
{
    FusTraceTree trace = NULL;
    if (unlikely(!hidr || count == 0)) return NULL;
    FUSB_GET_TRACE_FUSION(api, &trace);

    size_t size_buffer = 0;
    for (size_t i = 0; i < count; i++) size_buffer += X86DraticCase(&hidr[i]);

    FusBackendGenerateDataBlock_t *block = FUSB_CREATE_BLOCK(api, 23, size_buffer);
    if (unlikely(!block)) {
        FUS_PUSH_ERR(trace, FUSION_OK, "Backend Create BlockCompiler, Fail");
        return NULL;
    }

    FusBackendTransferLifetime_t *transfer = FUSB_CREATE_TRASNFER(api, block, DestroyLifetimeBlock);
    if (unlikely(!transfer)) {
        DestroyLifetimeBlock(block);
        FUS_PUSH_ERR(trace, FUSION_ERRO, "Backend Create TransferLifetime Block, Fail");
        return NULL;
    }

    for (size_t i = 0; i < count; i++) {
        if (count > 32 && i + 8 < count) __builtin_prefetch(&hidr[i + 8], 0, 2);
        FusStatusFlag_t flag = X86_ProcessOnceHidr(api, block, &hidr[i]);
        if (flag != FUSION_OK) {
            block->flag = flag;
            FUS_PUSH_ERR(trace, FUSION_OK, "Backend Generation Boundary Reached");
            return transfer;
        }
    }

    block->flag = FUSION_OK;
    FUS_PUSH_ERR(trace, FUSION_OK, "Backend Full Generation ByteCode Step Completed");
    return transfer;
}

/* ============================================================================
 * Linker helpers
 * ========================================================================= */
static inline void X86_WriteInt32(uint8_t *base, size_t offset, int32_t v)
{
    memcpy(base + offset, &v, sizeof(int32_t));
}

static FusStatusFlag_t X86_LinkerHelper(
    FusBackendApi_t *api, FusBackendRelocationOpaqueType_t type, FusBackendRelocContext_t *ctx
)
{
    FusTraceTree trace = NULL;
    if (unlikely(!ctx)) return FUSION_ERRO;
    FUSB_GET_TRACE_FUSION(api, &trace);

    switch ((X86ReallocTypes_t)type) {
        case X86_REL32: {
            int64_t delta = (int64_t)ctx->sym_addr - (int64_t)(ctx->patch_addr + 4);
            if (delta > INT32_MAX || delta < INT32_MIN) return FUSION_ERRO;
            X86_WriteInt32(ctx->buffer, ctx->offset, (int32_t)delta);
            FUS_PUSH_ERR(trace, FUSION_OK, "Backend, Relocation REL32 Resolver");
            return FUSION_OK;
        }
        case X86_ABS64: {
            *(uint64_t*)(ctx->buffer + ctx->offset) = (uint64_t)ctx->sym_addr;
            FUS_PUSH_ERR(trace, FUSION_OK, "Backend, Relocation ABS64 Resolver");
            return FUSION_OK;
        }
        default:
            FUS_PUSH_ERR(trace, FUSION_ERRO, "Backend, Relocation Unknown Type");
            return FUSION_ERRO;
    }
}

/* ============================================================================
 * Static backend interface
 * ========================================================================= */
#include <Internal/Backend/Fus_StaticBackend.h>

static FusBackendInterface_t interface = {
    .FUSI_BackendMountHidrArray   = X86_BackendMountHidrArry,
    .FUSI_BackendLinkerRelocation = X86_LinkerHelper,
};

FusBackendInterface_t* X86_BackendDefine(void)
{
    return &interface;
}
REGISTER_BACKEND(X86_Backend, X86_BackendDefine);