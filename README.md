# FusionBackendX86

Backend **x86 / x86_64** do projeto **Fusion**.

Ele recebe o **HIDR** (IR do Fusion Core) e gera **bytes de máquina x86_64** prontos para o linker do Core.
Atualmente o Core só suporta **link estático** de backends, então este repo gera um `.a` que é passado para o build do Core.

## O que faz

* Entry point: `src/x86_interface.c` → `X86_BackendDefine()` registrado como `X86_Backend` via `REGISTER_BACKEND`
* Seleção de família por `opcode + dst_type + src_type` (`src/x86_FamilyDefine.inc`, `src/x86_familys.h`)
* Monta `prefixo 0x66` (op_size 16) e `REX.W/R/B` (op_size 64 + R8–R15)
* Pipeline de encoding em `src/x86_pipeline.c`, na ordem que o processador espera:
  `prefix -> REX -> opcode -> ModRM -> SIB -> disp -> imm`
* Gerencia bloco de código (`FUSB_CREATE_BLOCK`) + lifetime (`FUSB_CREATE_TRASNFER`)
* Resolve relocações no linker:
  * `X86_REL32`: `sym_addr - (patch_addr + 4)` com check `INT32`
  * `X86_ABS64`: escreve `uint64_t` absoluto

## Instruções suportadas

Implementadas em `src/InstructionSets/`:

| HIDR | Arquivo | Exemplo gerado |
|------|---------|----------------|
| `MOV` | `x86_mov.c` | `MOV RBX, 90` → `48 BB 5A 00 00 00` |
| `ADD` | `x86_add.c` | `ADD RBX, RAX` → `48 01 C3` |
| `CALL` | `x86_call.c` | `CALL RAX` → `FF D0`, `CALL sym` com `REL32` |
| `RET` | `x86_ret.c` | `C3` |
| `ADDR` (LEA) | `x86_lea.c` | `LEA RBX, [RAX+16]` → `48 8D 58 10` |
| `PUSH` / `POP` | `x86_stack.c` | `PUSH RBX` → `48 53` |
| `CMP` | `x86_cmp.c` | `CMP RBX, RAX` → `48 3B D8` |
| `SYSCALL` | `x86_syscall.c` | `48 0F 05` |

Tamanhos: `8 / 16 / 32 / 64` (`HIDR_OP_SIZE_*`), imediatos `IMM8/16/32`, mem `disp0/disp8/disp32`.

Ver testes golden em `exemples/golden_encodingx86.c`.

## Estrutura

```
src/
  x86_interface.c        # entry point + REX/prefix + select family + linker
  x86_pipeline.c         # flatten x86Instruction_t -> bytes
  x86_types.h            # x86Instruction_t, ModRM, SIB, REX
  x86_functions.h
  x86_helpers.h          # mapa reg virtual Fusion -> hw (ACL0=RAX, BCL0=RBX, ..., GCL0-7=R8-R15)
  x86_familys.h
  x86_FamilyDefine.inc
  InstructionSets/       # um .c por família
exemples/
  golden_encodingx86.c   # teste estático
Makefile                 # gera fusbackendX86.a
```

## Build

```bash
make
# gera: fusbackendX86.a

make clean
```

Requer: `gcc`, `ar`, headers do Fusion Core (`-I../include` / `$CORE_PATCH/include`).

## Uso no Fusion Core (link estático)

Hoje o Core só aceita backend via **link estático**. Passe o `.a` com `STATICS_BACKENDS=`:

```bash
# 1. compile este backend
make

# 2. compile o Core linkando este backend estaticamente
make -C <CORE_PATCH> STATICS_BACKENDS=$(pwd)/fusbackendX86.a

# forma genérica pedida:
make STATICS_BACKENDS=patch_backend
# exemplo real:
# make -C ../FusionCore STATICS_BACKENDS=/home/ewerton-pc/Projetos/FusionBackendX86/fusbackendX86.a
```

O `Makefile` já tem um atalho (ajuste `CORE_PATCH=`):

```bash
CORE_PATCH=../FusionCore make example
# faz:
# $(MAKE) -C $(CORE_PATCH) STATICS_BACKENDS=$(pwd)/fusbackendX86.a
# gcc -I$(CORE_PATCH)/include exemples/golden_encodingx86.c -L$(CORE_PATCH)/ -lfusion -o golden_encoding
```

## Carregando no código

Depois do link estático, carregue pelo nome registrado:

```c
FusModuleBackend be = NULL;
fusLoaderBackend(inst, &be, "X86_Backend", FUS_BACKEND_TYPE_STATIC);

FusCommandBackend bc = {.sType = FUS_COMMAND_SEND_BACKEND, .pNext = NULL, .backend = be};
FusCommandHidr hc = {.sType = FUS_COMMAND_SEND_HIDR, .pNext = (FusCommandRuleBase_t*)&bc, .code = code};
fusMountHidrsBytes(inst, (FusCommandRuleBase_t*)&hc, &ret);
```

Exemplo completo: `exemples/golden_encodingx86.c`.
