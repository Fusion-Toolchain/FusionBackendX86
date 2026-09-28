/**
 * ███████╗██╗   ██╗███████╗██╗ ██████╗ ███╗   ██╗
 * ██╔════╝██║   ██║██╔════╝██║██╔═══██╗████╗  ██║
 * █████╗  ██║   ██║███████╗██║██║   ██║██╔██╗ ██║
 * ██╔══╝  ██║   ██║╚════██║██║██║   ██║██║╚██╗██║
 * ██║     ╚██████╔╝███████║██║╚██████╔╝██║ ╚████║
 * ╚═╝      ╚═════╝ ╚══════╝╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 * @file    x86_add.c
 * @brief   X86 ADD family encoders.
 * @author     Ewerton23929dev
 *
 * @details
 * Translates the register to register and immediate to register HIDR variants
 * into opcode, ModRM and size extensions, picking the shortest valid form.
 * @copyright  Copyright (c) 2026 Ewerton23929dev. All rights reserved.
 */

#include "../x86_helpers.h"
#include "x86_instructions.h"

bool X86_CaseMountAddRegReg(X86BackendContext* backend_ctx)
{
    const FusHidrNode_t* mir_node = backend_ctx->hidr;
    x86Instruction_t* mount_instr = backend_ctx->encoder;

    size_t src_reg = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir_node->src.data.reg));
    size_t dst_reg = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir_node->dst.data.reg));
    if (src_reg == (size_t)-1 || dst_reg == (size_t)-1) return false;

    if (mir_node->op_size == HIDR_OP_SIZE_8) {
        mount_instr->opcode.opcode[0] = 0x00;
    } else {
        mount_instr->opcode.opcode[0] = 0x01;
    }
    mount_instr->opcode.opcode_size = 1;

    mount_instr->modrm.mod = MODRM_MOD_REG_DIRECT;
    mount_instr->modrm.reg = src_reg & 0x7;
    mount_instr->modrm.rm  = dst_reg & 0x7;
    mount_instr->has_modrm = true;

    mount_instr->rex.w = (mir_node->op_size == HIDR_OP_SIZE_64) ? 1 : 0;
    mount_instr->rex.r = (src_reg >= 8);
    mount_instr->rex.b = (dst_reg >= 8);
    mount_instr->has_rex = (mount_instr->rex.w || mount_instr->rex.r || mount_instr->rex.b);

    return true;
}

bool X86_CaseMountAddImmReg(X86BackendContext* backend_ctx)
{
    const FusHidrNode_t* mir_node = backend_ctx->hidr;
    x86Instruction_t* mount_instr = backend_ctx->encoder;

    size_t dst_reg = X86_MapVirtualReg(FUS_HIDR_REG_INTERNAL(mir_node->dst.data.reg));
    if (dst_reg == (size_t)-1) return false;

    size_t imm_size = X86_CalMirImmSize(mir_node->src.data.imm.size);
    if (!imm_size) return false;

    // Select correct opcode for ADD r/m, imm semantics
    if (mir_node->op_size == HIDR_OP_SIZE_8) {
        mount_instr->opcode.opcode[0] = 0x80; // ADD r/m8, imm8
    } else if (imm_size == 1) {
        mount_instr->opcode.opcode[0] = 0x83; // ADD r/m16/32/64, imm8 sign-extended
    } else {
        mount_instr->opcode.opcode[0] = 0x81; // ADD r/m16/32/64, imm16/32
    }
    mount_instr->opcode.opcode_size = 1;

    mount_instr->modrm.mod = MODRM_MOD_REG_DIRECT;
    mount_instr->modrm.reg = 0; // /0 = ADD
    mount_instr->modrm.rm  = dst_reg & 0x7;
    mount_instr->has_modrm = true;

    mount_instr->rex.w = (mir_node->op_size == HIDR_OP_SIZE_64) ? 1 : 0;
    mount_instr->rex.b = (dst_reg >= 8);
    mount_instr->has_rex = (mount_instr->rex.w || mount_instr->rex.b);

    mount_instr->imm.value = mir_node->src.data.imm.imm;
    mount_instr->imm.size  = imm_size;
    mount_instr->has_imm   = true;

    return true;
}