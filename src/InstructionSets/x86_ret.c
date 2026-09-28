/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_ret.c
 * @brief   X86 RET instruction encoder.
 * @author     Ewerton23929dev
 *
 * @details
 * Emits the one byte near return (C3) and updates the code block metadata used
 * by the linker.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

/*
 * x86_ret.c — RET encoder (C3)
 */

#include "../x86_helpers.h"
#include "x86_instructions.h"

bool X86_MountRet(X86BackendContext *ctx)
{
    x86Instruction_t *enc = ctx->encoder;

    enc->opcode.opcode[0]   = 0xC3; // RET near
    enc->opcode.opcode_size = 1;

    return true;
}
