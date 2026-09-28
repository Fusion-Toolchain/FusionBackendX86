/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_functions.h
 * @brief   Declarations of the X86 encoding pipeline.
 * @author     Ewerton23929dev
 *
 * @details
 * Exposes conversion of a mounted instruction into bytes in the buffer, advancing
 * the offset on success, along with the emission helpers.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

#ifndef X86_BACKEND_FUNCTIONS
#define X86_BACKEND_FUNCTIONS

#include "x86_types.h"
#include <stddef.h>
#include <stdbool.h>

/*
 * Encode x86Instruction_t into buffer at *offset.
 * Advances *offset on success.
 */
bool X86_MountCodeBytes(x86Instruction_t *instr, size_t *offset, uint8_t *buffer, size_t buffer_size);

#endif
