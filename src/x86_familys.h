/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_familys.h
 * @brief   X86 instruction family dispatch.
 * @author     Ewerton23929dev
 *
 * @details
 * Declares the context handed to each encoder and the table mapping opcode and
 * operand types to the matching family, keeping routing in one place.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

#ifndef X86_INTERNAL_FAMILYS_H
#define X86_INTERNAL_FAMILYS_H

#include <Fusion/IRTypes/HidrType.h>
#include <Internal/Backend/Fus_Backend.h>
#include "x86_types.h"

/*
 * Backend context passed to each builder.
 */
typedef struct {
    x86Instruction_t              *encoder;
    const FusHidrNode_t           *hidr;
    FusBackendGenerateDataBlock_t *block;
    FusBackendApi_t               *Api;
} X86BackendContext;

/*
 * Family rule: maps (dst_type, src_type) to builder.
 */
typedef bool (*X86FmailyRuleFunc_t)(X86BackendContext*);

typedef struct {
    FusHidrOperandType_t src_type;
    FusHidrOperandType_t dst_type;
    X86FmailyRuleFunc_t  builder;
} X86FamilyRule_t;

typedef struct {
    X86FamilyRule_t   *rules;
    size_t             rule_count;
    FusHidrNodeKind_t  opcode;
} x86Familys_t;

#endif
