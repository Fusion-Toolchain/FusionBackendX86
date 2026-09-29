#include "x86_familys.h"
#include "x86_functions.h"
#include "x86_types.h"
#include "x86_subsystem.h"
#include "x86_sizes.h"

#include "InstructionSets/x86_instructions.h"
#include "x86_FamilyDefine.inc"

#include <stdio.h>

/* ============================================================================
 * Prefix / REX mount (per HIDR)
 * ========================================================================= */
static inline void X86_MountPrefixHidr(const FusHidrNode_t *mir, x86Instruction *instr)
{
    instr->has_prefix = false;
    instr->prefix.prefix_size = 0;

    // 0x66 for 16-bit operand size
    if (mir->op_size == HIDR_OP_SIZE_16) {
        instr->prefix.prefix[instr->prefix.prefix_size++] = 0x66;
        instr->has_prefix = true;
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
 * Lifetime & mount array
 * ========================================================================= */
static void DestroyLifetimeBlock(const void *data)
{
    FusBackendGenerateDataBlock_t *block = (FusBackendGenerateDataBlock_t*)data;
    FUSB_DESTROY_BLOCK(block->api, block);
}

FusBackendTransferLifetime_t* X86_BackendMountHidrArry(FusBackendApi_t *api, const FusHidrNode_t *hidr, const size_t count)
{
    FusTraceTree trace = NULL;
    if (unlikely(!hidr || count == 0)) return NULL;
    FUSB_GET_TRACE_FUSION(api, &trace);

    size_t size_buffer = 0;
    size_t need_reloc = 0;
    for (size_t i = 0; i < count; i++) {
        size_buffer += X86_SizeOfHidr(&hidr[i]);
        if (X86_NeedsReloc(&hidr[i])) need_reloc++;
    }

    FusBackendGenerateDataBlock_t *block =
        FUSB_CREATE_BLOCK(api, need_reloc ? need_reloc : 1, size_buffer);
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

        x86Instruction out = {0};
        X86_MountPrefixHidr(&hidr[i], &out);

        X86BackendContext ctx = {
            .encoder = &out,
            .hidr    = &hidr[i],
            .block   = block,
            .Api     = api,
        };

        if (unlikely(!X86_SelectFamily(api, &ctx))) {
            block->flag = FUSION_ERRO;
            FUS_PUSH_ERR(trace, FUSION_ERRO, "Backend Generation Boundary Reached");
            return transfer;
        }

        if (unlikely(!X86_MountCodeBytes(&out, &block->slab_offset, block->buffer_slab, block->slab_size))) {
            block->flag = FUSION_ERRO;
            FUS_PUSH_ERR(trace, FUSION_ERRO, "Backend Process Hidr Step-Fail");
            return transfer;
        }
    }

    block->flag = FUSION_OK;
    FUS_PUSH_ERR(trace, FUSION_OK, "Backend Full Generation ByteCode Step Completed");
    return transfer;
}