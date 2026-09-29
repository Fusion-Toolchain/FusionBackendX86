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

#include <Internal/Backend/Fus_Backend.h>
#include <Internal/Memory/Fus_Arena.h>
#include <Internal/Fus_TraceTree.h>
#include <BackendInterface/Backend.h>

#include "x86_subsystem.h"

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