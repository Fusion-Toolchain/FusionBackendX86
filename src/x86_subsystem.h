#ifndef X86_SUBSYSTEM_H
#define X86_SUBSYSTEM_H
#include <BackendInterface/Backend.h>

FusBackendTransferLifetime_t* X86_BackendMountHidrArry(FusBackendApi_t *api, const FusHidrNode_t *hidr, const size_t count);
FusStatusFlag_t X86_LinkerHelper(FusBackendApi_t *api, FusBackendRelocationOpaqueType_t type, FusBackendRelocContext_t *ctx);
#endif