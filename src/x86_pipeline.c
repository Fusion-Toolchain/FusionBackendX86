/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_pipeline.c
 * @brief   X86 encoding pipeline.
 * @author     Ewerton23929dev
 *
 * @details
 * Flattens the mounted instruction fields (prefixes, REX, opcode, ModRM, SIB,
 * displacement and immediate) into a byte sequence, consumed by the encoding
 * steps in the order the processor expects.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

/*
 * x86_pipeline.c — X86 encoding pipeline
 *
 *  Converts x86Instruction_t fields (prefix/REX/opcode/ModRM/SIB/disp/imm)
 *  into flat bytes via EncodeSteps.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "x86_types.h"
#include "x86_functions.h"

/* ---------------------------------------------------------------------------
 * Mounter context
 * ------------------------------------------------------------------------ */
struct CopyPartMemory {
    size_t        *offset;
    unsigned char *dst;
    size_t         dst_size;
    unsigned char *src;
    size_t         src_size;
};

typedef bool (*EncodeStep)(x86Instruction_t*, struct CopyPartMemory*);

typedef struct {
    EncodeStep steps[8];
    size_t     count;
} EncodePipeline;

/* ---------------------------------------------------------------------------
 * Low-level copy helpers
 * ------------------------------------------------------------------------ */
static bool CopyOffsetData(struct CopyPartMemory *m)
{
    if (!m->offset || !m->src || !m->dst) return false;
    if ((*m->offset) + m->src_size > m->dst_size) return false;
    memcpy(m->dst + *m->offset, m->src, m->src_size);
    *m->offset += m->src_size;
    return true;
}

static inline bool CopyOffsetDataU8(struct CopyPartMemory *m, uint8_t v)
{
    if (!m->offset || !m->dst) return false;
    if ((*m->offset) + 1 > m->dst_size) return false;
    m->dst[(*m->offset)++] = v;
    return true;
}

/* ---------------------------------------------------------------------------
 * Encode steps (one per field)
 * ------------------------------------------------------------------------ */
static bool x86Bytes_MountOpcode(x86Instruction_t *i, struct CopyPartMemory *m)
{
    m->src = i->opcode.opcode;
    m->src_size = i->opcode.opcode_size;
    return CopyOffsetData(m);
}

static inline uint8_t MountModRM(x86ModRm_t *modrm)
{
    if (modrm->mod > MODRM_MOD_MAX_VALUE ||
        modrm->reg > MODRM_REG_MAX_VALUE ||
        modrm->rm  > MODRM_RM_MAX_VALUE) {
        return 0;
    }
    return (modrm->mod << 6) | (modrm->reg << 3) | modrm->rm;
}

static bool x86Bytes_MountModRM(x86Instruction_t *i, struct CopyPartMemory *m)
{
    if (!i->has_modrm) return true;
    uint8_t modrm = MountModRM(&i->modrm);
    return CopyOffsetDataU8(m, modrm);
}

static bool x86Bytes_MountImm(x86Instruction_t *i, struct CopyPartMemory *m)
{
    if (!i->has_imm) return true;
    m->src = (unsigned char*)&i->imm.value;
    m->src_size = i->imm.size;
    return CopyOffsetData(m);
}

static bool x86Bytes_MountPrefix(x86Instruction_t *i, struct CopyPartMemory *m)
{
    if (!i->has_prefix) return true;
    m->src = i->prefix.prefix;
    m->src_size = i->prefix.prefix_size;
    return CopyOffsetData(m);
}

static inline uint8_t MountSIB(x86Sib_t *sib)
{
    return (sib->scale << 6) | (sib->index << 3) | sib->base;
}

static bool x86Bytes_MountSIB(x86Instruction_t *i, struct CopyPartMemory *m)
{
    if (!i->has_sib) return true;
    uint8_t sib = MountSIB(&i->sib);
    return CopyOffsetDataU8(m, sib);
}

static bool x86Bytes_MountDisp(x86Instruction_t *i, struct CopyPartMemory *m)
{
    if (!i->has_disp) return true;
    m->src = (unsigned char*)&i->disp.value;
    m->src_size = i->disp.size;
    return CopyOffsetData(m);
}

static bool x86Bytes_MountRex(x86Instruction_t *i, struct CopyPartMemory *m)
{
    if (!i->has_rex) return true;
    uint8_t rex = 0x40 | (i->rex.w << 3) | (i->rex.r << 2) | (i->rex.x << 1) | (i->rex.b << 0);
    return CopyOffsetDataU8(m, rex);
}

/* ---------------------------------------------------------------------------
 * Pipeline definition (order matters: prefix -> REX -> opcode -> ModRM/SIB/disp/imm)
 * ------------------------------------------------------------------------ */
static EncodePipeline pipeline_funcs = {
    .steps = {
        [0] = x86Bytes_MountPrefix,
        [1] = x86Bytes_MountRex,
        [2] = x86Bytes_MountOpcode,
        [3] = x86Bytes_MountModRM,
        [4] = x86Bytes_MountSIB,
        [5] = x86Bytes_MountDisp,
        [6] = x86Bytes_MountImm,
    },
    .count = 7,
};

/* ---------------------------------------------------------------------------
 * Public API: mount instruction bytes into buffer
 * ------------------------------------------------------------------------ */
bool X86_MountCodeBytes(x86Instruction_t *instr, size_t *offset, uint8_t *buffer, size_t buffer_size)
{
    if (!instr || !buffer || !offset) return false;

    struct CopyPartMemory ctx = {
        .dst      = buffer,
        .dst_size = buffer_size,
        .offset   = offset,
    };

    for (size_t i = 0; i < pipeline_funcs.count; i++) {
        if (!pipeline_funcs.steps[i](instr, &ctx)) return false;
    }
    return true;
}
