#include "x86_subsystem.h"
#include "x86_types.h"

#include <string.h>

/* ============================================================================
 * Linker helpers
 * ========================================================================= */
static inline void X86_WriteInt32(uint8_t *base, size_t offset, int32_t v)
{
    memcpy(base + offset, &v, sizeof(int32_t));
}
FusStatusFlag_t X86_LinkerHelper(FusBackendApi_t *api, FusBackendRelocationOpaqueType_t type, FusBackendRelocContext_t *ctx)
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