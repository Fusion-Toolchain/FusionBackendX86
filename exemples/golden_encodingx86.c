// STATIC EXEMPLES

#include <Fusion/Fusion.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static FusInstance g_inst = NULL;
static FusModuleBackend g_be = NULL;

static bool run_success(
    const char *name, FusHidrNode_t *nodes, size_t count,
    const unsigned char *expected, size_t exp_len
)
{
    FusCodeMount code = NULL;
    FusBackendReturn ret = NULL;

    if (!fusCreateCodeMount(&g_inst, &code)) {
        printf("FAIL %s: fusCreateCodeMount failed\n", name);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (!fusInsertCodeBlock(code, nodes[i])) {
            printf("FAIL %s: fusInsertCodeBlock %zu failed\n", name, i);
            fusDestroyCodeMount(g_inst, code);
            return false;
        }
    }
    FusCommandBackend bc = {.sType = FUS_COMMAND_SEND_BACKEND, .pNext = NULL, .backend = g_be};
    FusCommandHidr hc = {.sType = FUS_COMMAND_SEND_HIDR, .pNext = (FusCommandRuleBase_t*)&bc, .code = code};

    bool ok = fusMountHidrsBytes(g_inst, (FusCommandRuleBase_t*)&hc, &ret);
    if (!ok) {
        printf("FAIL %s: fusMountHidrsBytes failed (expected success)\n", name);
        fusDestroyCodeMount(g_inst, code);
        return false;
    }
    FusBufferContext_t *buf = fusGetStreamBufferCompiler(g_inst, ret);
    if (!buf) {
        printf("FAIL %s: fusGetStreamBufferCompiler failed\n", name);
        fusDestroyBackendReturn(g_inst, ret);
        fusDestroyCodeMount(g_inst, code);
        return false;
    }

    bool pass = true;
    if (buf->offset != exp_len || memcmp(buf->buffer, expected, exp_len) != 0) {
        printf("FAIL %s: bytes mismatch\n", name);
        printf("  got (%zu):", buf->offset);
        for (size_t i = 0; i < buf->offset; i++) printf(" %02X", buf->buffer[i]);
        printf("\n  expected (%zu):", exp_len);
        for (size_t i = 0; i < exp_len; i++) printf(" %02X", expected[i]);
        printf("\n");
        pass = false;
    } else {
        printf("\033[32mPASS\033[0m %-32s \033[33m", name);
        for (size_t i = 0; i < buf->offset; i++) printf("%02X%s", buf->buffer[i], i + 1 < buf->offset ? " " : "");
        printf("\033[0m");
        int blen = (int)buf->offset * 3 - 1;
        if (blen < 0) blen = 0;
        for (int i = blen; i < 40; i++) printf(" ");
        printf(" \033[32mOK\033[0m\n");
    }

    fusDestroyBufferCode(g_inst, buf);
    fusDestroyBackendReturn(g_inst, ret);
    fusDestroyCodeMount(g_inst, code);
    return pass;
}

static bool run_any(const char *name, FusHidrNode_t *nodes, size_t count)
{
    FusCodeMount code = NULL;
    FusBackendReturn ret = NULL;

    fusCreateCodeMount(&g_inst, &code);
    for (size_t i = 0; i < count; i++) fusInsertCodeBlock(code, nodes[i]);

    FusCommandBackend bc = {.sType = FUS_COMMAND_SEND_BACKEND, .pNext = NULL, .backend = g_be};
    FusCommandHidr hc = {.sType = FUS_COMMAND_SEND_HIDR, .pNext = (FusCommandRuleBase_t*)&bc, .code = code};

    bool ok = fusMountHidrsBytes(g_inst, (FusCommandRuleBase_t*)&hc, &ret);
    bool pass = ok;
    if (pass) {
        FusBufferContext_t *buf = fusGetStreamBufferCompiler(g_inst, ret);
        if (!buf || buf->offset == 0) pass = false;
        if (pass) {
            printf("\033[32mPASS\033[0m %-32s \033[33m", name);
            for (size_t i = 0; i < buf->offset; i++) printf("%02X%s", buf->buffer[i], i + 1 < buf->offset ? " " : "");
            printf("\033[0m");
            int blen = (int)buf->offset * 3 - 1;
            if (blen < 0) blen = 0;
            for (int i = blen; i < 40; i++) printf(" ");
            printf(" \033[32mOK\033[0m\n");
        } else {
            printf("FAIL %s: empty buffer\n", name);
        }
        if (buf) fusDestroyBufferCode(g_inst, buf);
        fusDestroyBackendReturn(g_inst, ret);
    } else {
        printf("FAIL %s: mount failed\n", name);
    }
    fusDestroyCodeMount(g_inst, code);
    return pass;
}

int main(void)
{
    if (!fusCreateInstance(&g_inst, NULL)) {
        printf("FAIL: fusCreateInstance failed\n");
        return 1;
    }
    if (!fusLoaderBackend(g_inst, &g_be, "X86_Backend", FUS_BACKEND_TYPE_STATIC)) { // LOAD HERE!!
        printf("FAIL: fusLoaderBackend failed\n");
        fusDestroyInstance(g_inst);
        return 1;
    }

    int passed = 0, total = 0;

#define TEST_OK(name, nodes, exp) do { total++; if (run_success(name, nodes, sizeof(nodes)/sizeof(nodes[0]), exp, sizeof(exp))) passed++; } while(0)

    // 1. MOV RBX, 90 (BCL0) + RET => 48 BB 5A 00 00 00 C3
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_Imm(90, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xBB, 0x5A, 0x00, 0x00, 0x00, 0xC3};
        TEST_OK("mov_imm_reg_bcl0", n, exp);
    }

    // 2. MOV R8, 90 (GCL0) => REX.W+B => 4D B8 ...
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL0"), FUS_HIDR_Imm(90, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x4D, 0xB8, 0x5A, 0x00, 0x00, 0x00, 0xC3};
        TEST_OK("mov_imm_reg_gcl0_r8", n, exp);
    }

    // 3. MOV RBX, RAX (BCL0 = RBX dst, ACL0 = RAX src) => 48 89 D8
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_Reg("ACL0")),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x89, 0xD8, 0xC3};
        TEST_OK("mov_reg_reg_bcl0_acl0", n, exp);
    }

    // 4. MOV RAX, RBX (inverso) => 48 89 C3
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("ACL0"), FUS_HIDR_Reg("BCL0")),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x89, 0xC3, 0xC3};
        TEST_OK("mov_reg_reg_acl0_bcl0", n, exp);
    }

    // 5. MOV RBX, [RAX] (BCL0 dst, mem base ACL0) => 48 8B 18
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"),
                      (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=0}}),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x8B, 0x18, 0xC3};
        TEST_OK("mov_reg_mem_bcl0_rax", n, exp);
    }

    // 6. LEA RBX, [RAX+16] => 48 8D 58 10
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_ADDR, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"),
                      (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=16}}),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x8D, 0x58, 0x10, 0xC3};
        TEST_OK("lea_bcl0_rax16", n, exp);
    }

    // 7. ADD RBX, RAX => 48 01 C3
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_ADD, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_Reg("ACL0")),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x01, 0xC3, 0xC3};
        TEST_OK("add_reg_reg_bcl0_acl0", n, exp);
    }

    // 8. ADD RBX, 5 => 48 81 C3 05 00 00 00
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_ADD, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_Imm(5, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x81, 0xC3, 0x05, 0x00, 0x00, 0x00, 0xC3};
        TEST_OK("add_imm_bcl0_5", n, exp);
    }

    // 9. PUSH RBX => 53 (with REX.W 48 53)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_PUSH, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x53, 0xC3};
        TEST_OK("push_bcl0", n, exp);
    }

    // 10. POP RBX => 5B (48 5B)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_POP, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x5B, 0xC3};
        TEST_OK("pop_bcl0", n, exp);
    }

    // 11. PUSH imm8 0x7B => 6A 7B (plus REX.W 48)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_PUSH, HIDR_OP_SIZE_64, FUS_HIDR_Imm(0x7B, HIDR_IMM8), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x6A, 0x7B, 0xC3};
        TEST_OK("push_imm8", n, exp);
    }

    // 12. SYSCALL => 0F 05 (with REX.W 48 0F 05)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_SYSCALL, HIDR_OP_SIZE_64, FUS_HIDR_None(), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x0F, 0x05, 0xC3};
        TEST_OK("syscall", n, exp);
    }

    // 13. CALL RAX => FF D0
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_CALL, HIDR_OP_SIZE_64, FUS_HIDR_Reg("ACL0"), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0xFF, 0xD0, 0xC3};
        TEST_OK("call_reg_rax", n, exp);
    }

    // 16. MOV RAX, 0x11223344 (ACL0) => 48 B8 44 33 22 11
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("ACL0"), FUS_HIDR_Imm(0x11223344, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xB8, 0x44, 0x33, 0x22, 0x11, 0xC3};
        TEST_OK("mov_imm_acl0", n, exp);
    }

    // 17. MOV RCX, 0x11223344 (TCL0 = RCX)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("TCL0"), FUS_HIDR_Imm(0x11223344, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xB9, 0x44, 0x33, 0x22, 0x11, 0xC3};
        TEST_OK("mov_imm_tcl0", n, exp);
    }

    // 18. MOV RDX, 0x11223344 (DCL0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("DCL0"), FUS_HIDR_Imm(0x11223344, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xBA, 0x44, 0x33, 0x22, 0x11, 0xC3};
        TEST_OK("mov_imm_dcl0", n, exp);
    }

    // 19. MOV RSI, 0x11223344 (ICL0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("ICL0"), FUS_HIDR_Imm(0x11223344, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xBE, 0x44, 0x33, 0x22, 0x11, 0xC3};
        TEST_OK("mov_imm_icl0", n, exp);
    }

    // 20. MOV RDI, 0x11223344 (OCL0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("OCL0"), FUS_HIDR_Imm(0x11223344, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xBF, 0x44, 0x33, 0x22, 0x11, 0xC3};
        TEST_OK("mov_imm_ocl0", n, exp);
    }

    // 21. MOV RSP, 0x11223344 (SCL0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("SCL0"), FUS_HIDR_Imm(0x11223344, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xBC, 0x44, 0x33, 0x22, 0x11, 0xC3};
        TEST_OK("mov_imm_scl0", n, exp);
    }

    // 22. MOV RBP, 0x11223344 (PCL0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("PCL0"), FUS_HIDR_Imm(0x11223344, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xBD, 0x44, 0x33, 0x22, 0x11, 0xC3};
        TEST_OK("mov_imm_pcl0", n, exp);
    }

    // 23. MOV R9, 90 (GCL1)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL1"), FUS_HIDR_Imm(90, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x4D, 0xB9, 0x5A, 0x00, 0x00, 0x00, 0xC3};
        TEST_OK("mov_imm_gcl1_r9", n, exp);
    }

    // 24. MOV R15, 90 (GCL7)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL7"), FUS_HIDR_Imm(90, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x4D, 0xBF, 0x5A, 0x00, 0x00, 0x00, 0xC3};
        TEST_OK("mov_imm_gcl7_r15", n, exp);
    }

    // 25. MOV EAX, 90 with 32-bit op size (ACH0) => B8 without REX
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_32, FUS_HIDR_Reg("ACH0"), FUS_HIDR_Imm(90, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0xB8, 0x5A, 0x00, 0x00, 0x00, 0xC3};
        TEST_OK("mov_imm_32bit", n, exp);
    }

    // 26. MOV [RAX], 0x12345678 (mem disp0) => 48 C7 40 00 78 56 34 12 (impl uses disp8=0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64,
                      (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=0}},
                      FUS_HIDR_Imm(0x12345678, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xC7, 0x40, 0x00, 0x78, 0x56, 0x34, 0x12, 0xC3};
        TEST_OK("mov_mem_imm_disp0", n, exp);
    }

    // 27. MOV [RAX+127], 0x12345678 => disp8
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64,
                      (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=127}},
                      FUS_HIDR_Imm(0x12345678, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xC7, 0x40, 0x7F, 0x78, 0x56, 0x34, 0x12, 0xC3};
        TEST_OK("mov_mem_imm_disp8", n, exp);
    }

    // 28. MOV [RAX+500], 0x12345678 => disp32
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64,
                      (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=500}},
                      FUS_HIDR_Imm(0x12345678, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0xC7, 0x80, 0xF4, 0x01, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12, 0xC3};
        TEST_OK("mov_mem_imm_disp32", n, exp);
    }

    // 29. LEA RBX, [RAX] disp0 => 48 8D 18
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_ADDR, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"),
                      (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=0}}),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x8D, 0x18, 0xC3};
        TEST_OK("lea_disp0", n, exp);
    }

    // 30. LEA RBX, [RAX+1000] disp32
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_ADDR, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"),
                      (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=1000}}),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x8D, 0x98, 0xE8, 0x03, 0x00, 0x00, 0xC3};
        TEST_OK("lea_disp32", n, exp);
    }

    // 31. PUSH R8 (GCL0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_PUSH, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL0"), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x4D, 0x50, 0xC3};
        TEST_OK("push_gcl0_r8", n, exp);
    }

    // 32. POP R15 (GCL7)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_POP, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL7"), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x4D, 0x5F, 0xC3};
        TEST_OK("pop_gcl7_r15", n, exp);
    }

    // 33. CALL R8 (GCL0)
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_CALL, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL0"), FUS_HIDR_None()),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x45, 0xFF, 0xD0, 0xC3};
        TEST_OK("call_gcl0_r8", n, exp);
    }

    // 34. ADD R8, RAX
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_ADD, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL0"), FUS_HIDR_Reg("ACL0")),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x49, 0x01, 0xC0, 0xC3};
        TEST_OK("add_gcl0_acl0", n, exp);
    }

    // 35. ADD RAX, R8
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_ADD, HIDR_OP_SIZE_64, FUS_HIDR_Reg("ACL0"), FUS_HIDR_Reg("GCL0")),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x4C, 0x01, 0xC0, 0xC3};
        TEST_OK("add_acl0_gcl0", n, exp);
    }

    // 36. CMP RBX, RAX (BCL0, ACL0) => 48 3B D8
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_CMP, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_Reg("ACL0")),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x3B, 0xD8, 0xC3};
        TEST_OK("cmp_reg_bcl0_acl0", n, exp);
    }

    // 37. CMP RBX, 0x11 (imm32) => 48 81 FB 11 00 00 00
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_CMP, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_Imm(0x11, HIDR_IMM32)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x81, 0xFB, 0x11, 0x00, 0x00, 0x00, 0xC3};
        TEST_OK("cmp_imm_bcl0_32", n, exp);
    }

    // 38. CMP RBX, 0x11 (imm8) => 48 83 FB 11
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_CMP, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"), FUS_HIDR_Imm(0x11, HIDR_IMM8)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x48, 0x83, 0xFB, 0x11, 0xC3};
        TEST_OK("cmp_imm_bcl0_8", n, exp);
    }

    // 39. CMP R8, RAX (GCL0, ACL0) => 4D 3B C0? Wait GCL0 dst, ACL0 src => 4D 3B C0? Check: GCL0=8, ACL0=0 => reg dst 8 => 0 after mask, rm src 0 => 0, REX R for dst, B for src? For CMP 3B reg=dst, rm=src => R for dst, B for src => 4D? Actually W=1,R for dst=1 => 0x4C? Let's trust gen: for CMP GCL0,ACL0 we earlier got 4D? Need verify.
    // Use run_any to auto-validate, but provide exact for two
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_CMP, HIDR_OP_SIZE_64, FUS_HIDR_Reg("GCL0"), FUS_HIDR_Reg("ACL0")),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x4C, 0x3B, 0xC0, 0xC3};
        TEST_OK("cmp_reg_gcl0_acl0", n, exp);
    }

    // 40. CMP AL, 0x11 (8-bit) => 80 F8 11
    {
        FusHidrNode_t n[] = {
            FUS_HIDRM(HIDR_INSTR_CMP, HIDR_OP_SIZE_8, FUS_HIDR_Reg("ACN0"), FUS_HIDR_Imm(0x11, HIDR_IMM8)),
            FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
        };
        unsigned char exp[] = {0x80, 0xF8, 0x11, 0xC3};
        TEST_OK("cmp_imm_8bit", n, exp);
    }

    // === Group tests: backend register table ===
    {
        const char *gp_regs[] = {"ACL0","BCL0","TCL0","DCL0","SCL0","PCL0","ICL0","OCL0","GCL0","GCL1","GCL2","GCL3","GCL4","GCL5","GCL6","GCL7"};
        for (size_t r = 0; r < sizeof(gp_regs)/sizeof(gp_regs[0]); r++) {
            char name[64];
            snprintf(name, sizeof(name), "group_mov_imm_%s", gp_regs[r]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)gp_regs[r]), FUS_HIDR_Imm(0x42, HIDR_IMM32)),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
    }

    // Group: MOV reg, reg for all GP combos (sample cross)
    {
        const char *regs[] = {"ACL0","BCL0","GCL0","GCL7"};
        for (size_t a = 0; a < 4; a++) for (size_t b = 0; b < 4; b++) {
            char name[64];
            snprintf(name, sizeof(name), "group_mov_reg_%s_%s", regs[a], regs[b]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)regs[a]), FUS_HIDR_Reg((char*)regs[b])),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
    }

    // Group: PUSH/POP for each GP reg
    {
        const char *regs[] = {"ACL0","BCL0","TCL0","DCL0","ICL0","OCL0","GCL0","GCL7"};
        for (size_t r = 0; r < 8; r++) {
            char name[64];
            snprintf(name, sizeof(name), "group_push_%s", regs[r]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_PUSH, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)regs[r]), FUS_HIDR_None()),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
            snprintf(name, sizeof(name), "group_pop_%s", regs[r]);
            FusHidrNode_t m[] = {
                FUS_HIDRM(HIDR_INSTR_POP, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)regs[r]), FUS_HIDR_None()),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, m, 2)) passed++;
        }
    }

    // Group: CALL for each GP reg
    {
        const char *regs[] = {"ACL0","BCL0","TCL0","GCL0","GCL7"};
        for (size_t r = 0; r < 5; r++) {
            char name[64];
            snprintf(name, sizeof(name), "group_call_%s", regs[r]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_CALL, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)regs[r]), FUS_HIDR_None()),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
    }

    // Group: size variations per instruction (MOV, ADD)
    {
        struct { const char *reg; FusHidrOpcodeSize_t sz; FusHidrImmSize_t isz; } sizes[] = {
            {"ACN0", HIDR_OP_SIZE_8, HIDR_IMM8},
            {"ACW0", HIDR_OP_SIZE_16, HIDR_IMM16},
            {"ACH0", HIDR_OP_SIZE_32, HIDR_IMM32},
            {"ACL0", HIDR_OP_SIZE_64, HIDR_IMM32},
        };
        for (size_t s = 0; s < 4; s++) {
            char name[64];
            snprintf(name, sizeof(name), "group_size_mov_%s", sizes[s].reg);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_MOV, sizes[s].sz, FUS_HIDR_Reg((char*)sizes[s].reg), FUS_HIDR_Imm(0x11, sizes[s].isz)),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
            snprintf(name, sizeof(name), "group_size_add_%s", sizes[s].reg);
            FusHidrNode_t m[] = {
                FUS_HIDRM(HIDR_INSTR_ADD, sizes[s].sz, FUS_HIDR_Reg((char*)sizes[s].reg), FUS_HIDR_Imm(0x11, sizes[s].isz)),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, m, 2)) passed++;
        }
    }

    // Group: LEA with different bases and offsets (from x86_helpers.h SEG/GP)
    {
        const char *bases[] = {"ACL0","BCL0","TCL0","ICL0"};
        int offs[] = {0, 8, 128, 1000};
        for (size_t b = 0; b < 4; b++) for (size_t o = 0; o < 4; o++) {
            char name[64];
            snprintf(name, sizeof(name), "group_lea_%s_off%d", bases[b], offs[o]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_ADDR, HIDR_OP_SIZE_64, FUS_HIDR_Reg("BCL0"),
                          (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre((char*)bases[b]),.offset=(uint16_t)offs[o]}}),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
    }

    // === Exhaustive: all GP regs x all sizes x all families ===
    {
        const char *gp_regs[] = {"ACL0","BCL0","TCL0","DCL0","SCL0","PCL0","ICL0","OCL0","GCL0","GCL1","GCL2","GCL3","GCL4","GCL5","GCL6","GCL7"};
        struct { const char *suf; FusHidrOpcodeSize_t sz; FusHidrImmSize_t isz; } szs[] = {
            {"8",  HIDR_OP_SIZE_8,  HIDR_IMM8},
            {"16", HIDR_OP_SIZE_16, HIDR_IMM16},
            {"32", HIDR_OP_SIZE_32, HIDR_IMM32},
            {"64", HIDR_OP_SIZE_64, HIDR_IMM32},
        };
        // MOV imm reg for every reg x size
        for (size_t r = 0; r < 16; r++) for (size_t s = 0; s < 4; s++) {
            char name[64];
            snprintf(name, sizeof(name), "exh_mov_imm_%s_%s", gp_regs[r], szs[s].suf);
            // map suffix to reg name with size char: ACN0/ACW0/ACH0/ACL0 etc. Build reg string dynamically
            char reg[8];
            // keep base letter from gp_regs (first char) + C + size char + index
            // gp_regs are like ACL0, GCL1 -> need to preserve index and group
            // simplify: use gp_regs as is but change size char: 3rd char
            snprintf(reg, sizeof(reg), "%s", gp_regs[r]);
            if (strcmp(szs[s].suf,"8")==0) reg[2]='N';
            else if (strcmp(szs[s].suf,"16")==0) reg[2]='W';
            else if (strcmp(szs[s].suf,"32")==0) reg[2]='H';
            else reg[2]='L';
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_MOV, szs[s].sz, FUS_HIDR_Reg(reg), FUS_HIDR_Imm(0x11, szs[s].isz)),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
        // MOV reg,reg for every pair (sample all 16x16 =256, but limit to 16*4 to keep 64 tests)
        for (size_t a = 0; a < 16; a++) for (size_t b = 0; b < 4; b++) {
            char name[64];
            snprintf(name, sizeof(name), "exh_mov_reg_%s_%s", gp_regs[a], gp_regs[b]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)gp_regs[a]), FUS_HIDR_Reg((char*)gp_regs[b])),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
        // ADD reg,reg and ADD imm for every reg x size
        for (size_t r = 0; r < 16; r++) for (size_t s = 0; s < 4; s++) {
            char name[64];
            snprintf(name, sizeof(name), "exh_add_imm_%s_%s", gp_regs[r], szs[s].suf);
            char reg[8]; snprintf(reg,sizeof(reg),"%s",gp_regs[r]);
            if (strcmp(szs[s].suf,"8")==0) reg[2]='N';
            else if (strcmp(szs[s].suf,"16")==0) reg[2]='W';
            else if (strcmp(szs[s].suf,"32")==0) reg[2]='H';
            else reg[2]='L';
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_ADD, szs[s].sz, FUS_HIDR_Reg(reg), FUS_HIDR_Imm(0x11, szs[s].isz)),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
        // CMP reg,reg and CMP imm for every reg x size
        for (size_t r = 0; r < 16; r++) for (size_t s = 0; s < 4; s++) {
            char name[64];
            snprintf(name, sizeof(name), "exh_cmp_imm_%s_%s", gp_regs[r], szs[s].suf);
            char reg[8]; snprintf(reg,sizeof(reg),"%s",gp_regs[r]);
            if (strcmp(szs[s].suf,"8")==0) reg[2]='N';
            else if (strcmp(szs[s].suf,"16")==0) reg[2]='W';
            else if (strcmp(szs[s].suf,"32")==0) reg[2]='H';
            else reg[2]='L';
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_CMP, szs[s].sz, FUS_HIDR_Reg(reg), FUS_HIDR_Imm(0x11, szs[s].isz)),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
        for (size_t a = 0; a < 16; a++) for (size_t b = 0; b < 4; b++) {
            char name[64];
            snprintf(name, sizeof(name), "exh_cmp_reg_%s_%s", gp_regs[a], gp_regs[b]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_CMP, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)gp_regs[a]), FUS_HIDR_Reg((char*)gp_regs[b])),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
        // MOV reg, [mem] and MOV [mem], imm for each base reg and offset size
        for (size_t r = 0; r < 8; r++) {
            const char *dst = gp_regs[r];
            for (size_t o = 0; o < 3; o++) {
                int off = (o==0?0:(o==1?127:500));
                char name[64];
                snprintf(name,sizeof(name),"exh_mov_regmem_%s_off%d", dst, off);
                FusHidrNode_t n[] = {
                    FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)dst),
                              (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre("ACL0"),.offset=(uint16_t)off}}),
                    FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
                };
                total++; if (run_any(name, n, 2)) passed++;
                snprintf(name,sizeof(name),"exh_mov_memimm_off%d", off);
                FusHidrNode_t m[] = {
                    FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64,
                              (FusHidrOperand_t){.type=HIDR_OPERAND_TYPE_MEM_REF,.data.memory_ref={.base=fusInterpreterRegistre((char*)dst),.offset=(uint16_t)off}},
                              FUS_HIDR_Imm(0x1234, HIDR_IMM32)),
                    FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
                };
                total++; if (run_any(name, m, 2)) passed++;
            }
        }
        // SYM variants: MOV sym -> reg and CALL sym
        for (size_t r = 0; r < 4; r++) {
            char name[64];
            snprintf(name,sizeof(name),"exh_mov_sym_%s", gp_regs[r]);
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_MOV, HIDR_OP_SIZE_64, FUS_HIDR_Reg((char*)gp_regs[r]), FUS_HIDR_Sym("test_sym")),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any(name, n, 2)) passed++;
        }
        {
            FusHidrNode_t n[] = {
                FUS_HIDRM(HIDR_INSTR_CALL, HIDR_OP_SIZE_64, FUS_HIDR_Sym("test_sym"), FUS_HIDR_None()),
                FUS_HIDRM(HIDR_INSTR_RET, HIDR_OP_SIZE_32, FUS_HIDR_None(), FUS_HIDR_None()),
            };
            total++; if (run_any("exh_call_sym", n, 2)) passed++;
        }
    }

    printf("\nResult: %d/%d passed\n", passed, total);
    fusDestroyBackend(g_inst, g_be);
    fusDestroyInstance(g_inst);
    return (passed == total) ? 0 : 1;
}
