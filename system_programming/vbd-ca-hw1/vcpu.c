/*
 * Copyright (c) 2018   Soongsil University
 *
 * Authors: Doosol Lee, Hyoeun Lee, Kyujin Choi, and Kanghee Kim
 *               at Rubicom Lab (http://rubicom.ssu.ac.kr)
 *
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice, 
 *     this list of conditions and the following disclaimer in the documentation and/or 
 *     other materials provided with the distribution. 
 * 3. Neither the name of the copyright holder nor the names of its contributors 
 *     may be used to endorse or promote products derived from this software without 
 *     specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <errno.h>
#include "vcpu.h"
#include "elf.h"

///////////////////////////////////////////////////////////////////////////
// Macro definitions
///////////////////////////////////////////////////////////////////////////

#define INST_OPCODE_MOV	0xd
#define INST_OPCODE_CMP	0xa
#define INST_OPCODE_BX  0x9
#define INST_OPCODE_ADD	0x4
#define INST_OPCODE_SUB	0x2
#define INST_OPCODE_MUL 0x0

#define INST_OPCODE_PUSH 0x12
#define INST_OPCODE_POP  0x0b

#define INST_OPCODE_STORE 0b0
#define INST_OPCODE_LOAD  0b1

#define INST_SUBCODE_BX   0x1
#define INST_SUBCODE_MUL  0x9

#define INST_IMMEDIATE_VALUE(operand)  ((operand) & 0x00ff)
#define INST_IMMEDIATE_SHCNT(operand)  (((operand) & 0x0f00) << 1)
#define INST_OPERAND_SHCNT(operand)  (((operand) & 0x0f80) >> 7)
#define INST_OPERAND_SHTYP(operand)  (((operand) & 0x60) >> 5)
#define INST_OPERAND_SHREG(operand)  (((operand) & 0x10) >> 4)
#define INST_OPERAND_REGID(operand)  ((operand) & 0x0f)

ARM_CORE vcpu[4];
TASK_STRUCT *current[4];

int quit(char *tag){
  printf("%s error: %s (errno=%d)\n", tag, strerror(errno), errno);
  exit(1);
}

static int is_cond_met(CPSR cpsr, unsigned int cond) {
    switch (cond) {
    case 0: return cpsr.Z;
    case 1: return !cpsr.Z;
    case 2: return cpsr.C;
    case 3: return !cpsr.C;
    case 4: return cpsr.N;
    case 5: return !cpsr.N;
    case 6: return cpsr.V;
    case 7: return !cpsr.V;
    case 8: return cpsr.C & !cpsr.Z;
    case 9: return !cpsr.C | cpsr.Z;
    case 10: return !(cpsr.N ^ cpsr.V);
    case 11: return cpsr.N ^ cpsr.V;
    case 12: return !cpsr.Z & !(cpsr.N ^ cpsr.V);
    case 13: return cpsr.Z | (cpsr.N ^ cpsr.V);
    case 14: return 1;
    case 15: return 0;
    }
}

static void update_cpsr(CPSR *cpsr, int result, int operand1, int operand2) {
  cpsr->N = ((result >> 31) & 1ll) == 1 ? 1 : 0;
  cpsr->Z = (unsigned int) result == 0 ? 1 : 0;
  cpsr->C = result > 0xffffffff ? 1 : 0;
  cpsr->V = (((operand1 ^ operand2) >> 31) & 1) == 0
    && (((operand1 ^ result) >> 31) & 1ll) == 1 ? 1 : 0;
}

static void *prog_addr_to_host_addr(unsigned int vaddr, MM_STRUCT *mm) {
  void *host_addr;

  if (vaddr >= mm->text_vaddr && vaddr < mm->text_vaddr + mm->text_size)
    host_addr = mm->text_base + vaddr - mm->text_vaddr;
  else if (vaddr >= mm->data_vaddr && vaddr < mm->data_vaddr + mm->data_size)
    host_addr = mm->data_base + vaddr - mm->data_vaddr; // + mm->data_offset;
  else if (vaddr >= PROG_STACK_BASE && vaddr <= PROG_STACK_BASE + PROG_STACK_SIZE)
    host_addr = mm->stack_base + mm->stack_size + vaddr - mm->stack_iaddr;
  else {
    printf("unknown vaddr = 0x%x\n", vaddr);
    exit(1);
  }

  return host_addr;
}

static char *cond_suffix_strings[16] = {
  "eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
  "hi", "ls", "ge", "lt", "gt", "le", "",
};

static char *get_suffix(unsigned int cond) {
  return cond_suffix_strings[cond];
}

///////////////////////////////////////////////////////////////////////////
// Breakpoint handling
///////////////////////////////////////////////////////////////////////////

void set_breakpoint(TASK_STRUCT *task) {
  unsigned int vaddr = task->break_point;
  MM_STRUCT *mm = &task->mm;
  if (!(vaddr >= mm->text_vaddr && vaddr < mm->text_vaddr + mm->text_size)) {
    printf("The given breakpoint is outside the text segment.\n");
    return;
  }
  printf("set_breakpoint: vaddr = 0x%08x\n", vaddr);
  long long unsigned int host_addr = (long long unsigned int) prog_addr_to_host_addr(vaddr, &task->mm);

  if(mprotect(PAGE_ADDR(host_addr), PAGE_SIZE, PROT_READ|PROT_WRITE) < 0) 
    quit("mprotect");

  task->inst_at_bp = *((unsigned int *) host_addr);
  *((unsigned int *) host_addr) = TRAP_INST_CODE;

  if(mprotect(PAGE_ADDR(host_addr), PAGE_SIZE, PROT_READ) < 0) 
    quit("mprotect");

  return;
}

void reset_breakpoint(TASK_STRUCT *task) {
  unsigned int vaddr = task->break_point;
  printf("reset_breakpoint: vaddr = 0x%08x\n", vaddr);
  long long unsigned int host_addr = (long long unsigned int) prog_addr_to_host_addr(vaddr, &task->mm);

  if(mprotect(PAGE_ADDR(host_addr), PAGE_SIZE, PROT_READ|PROT_WRITE) < 0) 
    quit("mprotect");

  *((unsigned int *) host_addr) = task->inst_at_bp;

  if(mprotect(PAGE_ADDR(host_addr), PAGE_SIZE, PROT_READ) < 0) 
    quit("mprotect");

  return;
}

///////////////////////////////////////////////////////////////////////////
// Declaration of instruction type handlers
///////////////////////////////////////////////////////////////////////////
static void arm_inst_null_handler(ARM_CORE *core, unsigned int *inst); 
static void arm_inst_DPR_handler(ARM_CORE *core, unsigned int *inst); 
static void arm_inst_DPI_handler(ARM_CORE *core, unsigned int *inst); 
static void arm_inst_SDI_handler(ARM_CORE *core, unsigned int *inst); 
static void arm_inst_SDT_handler(ARM_CORE *core, unsigned int *inst); 
static void arm_inst_BDT_handler(ARM_CORE *core, unsigned int *inst); 
static void arm_inst_BRN_handler(ARM_CORE *core, unsigned int *inst); 

static void (*arm_inst_type_handlers[8])(ARM_CORE *, unsigned int *) = {
  arm_inst_DPR_handler, // [0]
  arm_inst_DPI_handler, // [1]
  arm_inst_SDI_handler, // [2]
  arm_inst_SDT_handler, // [3]
  arm_inst_BDT_handler, // [4]
  arm_inst_BRN_handler, // [5]
  arm_inst_null_handler, // [6]
  arm_inst_null_handler, // [7]
};


///////////////////////////////////////////////////////////////////////////
// Utility functions for bit manipulation
///////////////////////////////////////////////////////////////////////////
static unsigned int count_bits(int value){
  unsigned int cnt;
  for (cnt = 0; value != 0; value >>= 1) {
    if (value & 0x01) cnt++;	
  }
	
  return cnt;
}

static unsigned int get_bits(int value, int start, int len) {
    int val = 0, mask = 1;
    mask = ((1 << len) - 1) << (start - len);
    val = (mask & value) >> (start - len);
    return val;
}

static unsigned int shift_LSL(unsigned int bits, unsigned int count) {
  return bits << count; // C operator '<<' is equivalent to LSL
}

static unsigned int shift_ASR(unsigned int bits, unsigned int count) {
  return bits >> count; // C operator '>>' is equivalent to ASR
}

static unsigned int shift_LSR(unsigned int bits, unsigned int count) {
  if (count > 0) {
    bits >>= 1;
    if (((bits >> 31) & 1) == 1) bits -= 1 << 31;
    if (count > 1) bits >>= count - 1;
  }
  return bits;
}

static unsigned int shift_ROR(unsigned int bits, unsigned int count) {
  if (count > 0) {
    unsigned int tmp = get_bits(bits, count, count);
    bits = shift_LSR(bits, count);
    bits += tmp << (32 - count);
  }

  return bits;
}

///////////////////////////////////////////////////////////////////////////
// Definition of individual instruction handlers
///////////////////////////////////////////////////////////////////////////

static void arm_inst_null_handler(ARM_CORE *core, unsigned int *inst) {
  printf("unknown instruction (0x%x)\n", *inst);
  exit(1);
}

static void arm_inst_mov_handler(ARM_CORE *core, ARM_INST_DPR *inst, unsigned int operand) {
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    if (inst->type == 0)  printf("mov%s%s r%d, r%d\n", get_suffix(inst->cond), (inst->Sbit ? "s" : ""), inst->Rd, inst->Rm);
    else                          printf("mov%s%s r%d, #%d\n", get_suffix(inst->cond), (inst->Sbit ? "s" : ""), inst->Rd, operand);
  }

  if (!exec) return;

  core->reg.regs[inst->Rd] = operand;
}

static void arm_inst_cmp_handler(ARM_CORE *core, ARM_INST_DPR *inst, unsigned int operand) {
  long long oprnd1 = core->reg.regs[inst->Rn];
  unsigned long long oprnd2 = ~operand + 1;
  long long result = oprnd1 + oprnd2;
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    if (inst->type == 0)  printf("cmp%s r%d, r%d\n", get_suffix(inst->cond), inst->Rn, inst->Rm);
    else                          printf("cmp%s r%d, #%d\n", get_suffix(inst->cond), inst->Rn, operand);
  }

  if (!exec) return;

  if (inst->Sbit)
    update_cpsr(&core->reg.cpsr, oprnd1, oprnd2, result);
}

static void arm_inst_add_handler(ARM_CORE *core, ARM_INST_DPR *inst, unsigned int operand) {
  int oprnd1 = core->reg.regs[inst->Rn];
  int oprnd2 = operand;
  int result = oprnd1 + oprnd2;
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    if (inst->type == 0)  printf("add%s%s r%d, r%d, r%d\n", get_suffix(inst->cond), (inst->Sbit ? "s" : ""), inst->Rd, inst->Rn, inst->Rm);
    else                          printf("add%s%s r%d, r%d, #%d\n", get_suffix(inst->cond), (inst->Sbit ? "s" : ""), inst->Rd, inst->Rn, operand);
  }

  if (!exec) return;

  core->reg.regs[inst->Rd] = result;
  if (inst->Sbit)
    update_cpsr(&core->reg.cpsr, oprnd1, oprnd2, result);
}

static void arm_inst_sub_handler(ARM_CORE *core, ARM_INST_DPR *inst, unsigned int operand) {
  int oprnd1 = core->reg.regs[inst->Rn];
  int oprnd2 = ~operand + 1;
  int result = oprnd1 + oprnd2;
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    if (inst->type == 0)  printf("sub%s%s r%d, r%d, r%d\n", get_suffix(inst->cond), (inst->Sbit ? "s" : ""), inst->Rd, inst->Rn, inst->Rm);
    else                          printf("sub%s%s r%d, r%d, #%d\n", get_suffix(inst->cond), (inst->Sbit ? "s" : ""), inst->Rd, inst->Rn, operand);
  }

  if (!exec) return;

  core->reg.regs[inst->Rd] = result;
  if (inst->Sbit)
    update_cpsr(&core->reg.cpsr, oprnd1, oprnd2, result);
}

static void arm_inst_mul_handler(ARM_CORE *core, ARM_INST_DPR *inst) {
  long long oprnd1 = core->reg.regs[inst->Rm];
  unsigned int oprnd2 = core->reg.regs[inst->Rx];
  long long result = oprnd1 * oprnd2;
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    printf("mul%s%s r%d, r%d, r%d\n", get_suffix(inst->cond), (inst->Sbit ? "s" : ""), inst->Rn, inst->Rm, inst->Rx);
  }

  if (!exec) return;

  core->reg.regs[inst->Rn] = (unsigned int) result;
  if (inst->Sbit)
    update_cpsr(&core->reg.cpsr, oprnd1, oprnd2, result);
}

static void arm_inst_bx_handler(ARM_CORE *core, ARM_INST_BXW *inst) {
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    printf("bx%s  r%d;%x\n", get_suffix(inst->cond), inst->Rm, core->reg.regs[inst->Rm]);
  }

  if (!exec) return;

  core->reg.__PC__ = core->reg.regs[inst->Rm];
  core->reg.branch_taken = 1;
}

static void arm_inst_push_handler(ARM_CORE *core, ARM_INST_BDT *inst) {
  unsigned int reglist = inst->regs;
  unsigned int reg_id = 0;
  core->reg.__SP__ -= 4*count_bits(reglist);
  unsigned int *host_addr_of_SP = (unsigned int *) prog_addr_to_host_addr(core->reg.__SP__, core->mm);
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    printf("push%s {", get_suffix(inst->cond));
  }

  while (reglist != 0) {
    if (reglist & 0x01) {
      if (glob_verbose) printf("r%d,", reg_id); 
      if (exec) {
	*host_addr_of_SP = core->reg.regs[reg_id];
	host_addr_of_SP += 1; // increment by 1 means increment by 4 bytes.
      }
    }
    reglist >>= 1;
    reg_id++;
  }

  if (glob_verbose)  printf("}\n");
}

static void arm_inst_store_handler(ARM_CORE *core, ARM_INST_SDI *inst) {
  if (inst->Pbit == 0 && inst->Wbit == 1) {
    printf("NOT IMPLEMENTED STR (opcode = 0b%d%d%d%d%d, SEE STRT)\n",
      inst->Pbit, inst->Ubit, inst->Bbit, inst->Wbit, inst->Lbit);
    exit(1);
  }

  if (inst->Rn == 13 && inst->Pbit == 1 && inst->Ubit == 0 
      && inst->Wbit == 1 && inst->imm12 == 4) {
    ARM_INST_BDT inst2;
    inst2.regs = (1 << inst->Rt);
    inst2.Rn = 13;
    inst2.opcode = 0b10010;
    inst2.type = inst->type;
    inst2.cond = inst->cond;
    arm_inst_push_handler(core, &inst2);
    return;
  }

  unsigned int target_value = core->reg.regs[inst->Rt];
  unsigned int base_addr = core->reg.regs[inst->Rn];
  unsigned int offset = inst->imm12; //  t = UInt(Rt);  n = UInt(Rn);  imm32 = ZeroExtend(imm12, 32);
  unsigned int index = (inst->Pbit == 1);  
  unsigned int add = (inst->Ubit == 1);  
  unsigned int wback = ((inst->Pbit == 0) || (inst->Wbit == 1));

  unsigned int offset_addr = (add ? base_addr + offset : base_addr - offset); 
  unsigned int target_addr = (index ? offset_addr : base_addr);
  unsigned int *host_addr_of_target;
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    printf("str%s r%d, [r%d, #%d]\n", get_suffix(inst->cond), inst->Rt, inst->Rn, (add ? offset :  -offset));
  }

  if (!exec) return;

  host_addr_of_target = (unsigned int *) prog_addr_to_host_addr(target_addr, core->mm);
  *host_addr_of_target = (inst->Rt == 15 ? core->reg.__PC__ : target_value);
  if (wback) {
    core->reg.regs[inst->Rn] = offset_addr;
    if (inst->Rn == 15)  core->reg.branch_taken = 1;
  }
}

static void arm_inst_pop_handler(ARM_CORE *core, ARM_INST_BDT *inst) {
  unsigned int reglist = inst->regs;
  unsigned int reg_id = 0;
  unsigned int *host_addr_of_SP = (unsigned int *) prog_addr_to_host_addr(core->reg.__SP__, core->mm);
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    printf("pop%s {", get_suffix(inst->cond));
  }

  while (reglist != 0) {
    if (reglist & 0x01) { 
      if (glob_verbose) printf("r%d,", reg_id); 
      if (exec) {
	if(reg_id == 15) core->reg.branch_taken = 1; // doodu
	core->reg.regs[reg_id] = *host_addr_of_SP;
	core->reg.__SP__ += 4;
	host_addr_of_SP += 1; // increment by 1 means increment by 4 bytes.
      }
    }
    reglist >>= 1;
    reg_id++;
  }

  if (glob_verbose)  printf("}\n");
}

static void arm_inst_load_handler(ARM_CORE *core, ARM_INST_SDI *inst) {
  if (inst->Pbit == 0 && inst->Wbit == 1) {
    printf("NOT IMPLEMENTED STR (opcode = 0b%d%d%d%d%d, SEE LDRT)\n",
      inst->Pbit, inst->Ubit, inst->Bbit, inst->Wbit, inst->Lbit);
    exit(1);
  }

  if (inst->Rn == 13 && inst->Pbit == 0 && inst->Ubit == 1
      && inst->Wbit == 0 && inst->imm12 == 4) {
    ARM_INST_BDT inst2;
    inst2.regs = (1 << inst->Rt);
    inst2.Rn = 13;
    inst2.opcode = 0b01001;
    inst2.type = inst->type;
    inst2.cond = inst->cond;
    arm_inst_pop_handler(core, &inst2);
    return;
  }

  unsigned int base_addr = core->reg.regs[inst->Rn];
  unsigned int offset = inst->imm12; 
  unsigned int index = (inst->Pbit == 1);  
  unsigned int add = (inst->Ubit == 1);  
  unsigned int wback = ((inst->Pbit == 0) || (inst->Wbit == 1));

  unsigned int offset_addr = (add ? base_addr + offset : base_addr - offset); 
  unsigned int target_addr = (index ? offset_addr : base_addr);
  unsigned int *host_addr_of_target;
  unsigned int data;
  unsigned int exec = is_cond_met(core->reg.cpsr, inst->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    printf("ldr%s r%d, [r%d, #%d]\n", get_suffix(inst->cond), inst->Rt, inst->Rn, (add ? offset :  -offset));
  }

  if (!exec) return;

  host_addr_of_target = (unsigned int *) prog_addr_to_host_addr(target_addr, core->mm);
  data = *host_addr_of_target;

  if (wback) {
    core->reg.regs[inst->Rn] = offset_addr;
    if (inst->Rn == 15)  core->reg.branch_taken = 1;
  }

  if (inst->Rt == 15) {
    //if ((target_addr & 0x3) == 0) LoadWritePC(data);
    core->reg.regs[inst->Rt] = data;
    core->reg.branch_taken = 1;
  }
  else if ((target_addr & 0x3) == 0)
    core->reg.regs[inst->Rt] = data;
  else {
    printf("NOT IMPLEMENTED LDR: Can only apply before ARMv7\n");
    exit(1);
  }
}


///////////////////////////////////////////////////////////////////////////
// Definition of instruction type handlers
///////////////////////////////////////////////////////////////////////////
static unsigned int shift_operand(ARM_CORE *core, unsigned int operand) {
  unsigned int count = INST_OPERAND_SHCNT(operand);
  unsigned int type = INST_OPERAND_SHTYP(operand);
  unsigned int value = core->reg.regs[INST_OPERAND_REGID(operand)];

  if (INST_OPERAND_SHREG(operand)) 
    count = core->reg.regs[shift_LSR(count, 1)];

  switch (type) {
  case 0:  value = shift_LSL(value, count); break;
  case 1:  value = shift_LSR(value, count); break;
  case 2:  value = shift_ASR(value, count); break;
  case 3:  value = shift_ROR(value, count); break;
  }

  return value;
}

// handler for data-processing instructions
static void arm_inst_DPR_handler(ARM_CORE *core, unsigned int *inst) {
  ARM_INST_DPR *instr = (ARM_INST_DPR *) inst;
  unsigned int operand = ((ARM_INST_DPI *) inst)->operand2;
  switch(instr->opcode) {
  case INST_OPCODE_MUL:  
    if (instr->subcode == INST_SUBCODE_MUL)  
      arm_inst_mul_handler(core, instr);
    else printf("unknown MUL instruction (0x%x)\n", *inst);
    break;
  case INST_OPCODE_BX:
    if (instr->subcode == INST_SUBCODE_BX && instr->Sbit == 0 && 
	instr->Rn == 15 && instr->Rd == 15 && instr->Rx == 15) 
      arm_inst_bx_handler(core, (ARM_INST_BXW *) instr);
    else printf("unknown BX instruction (0x%x)\n", *inst);
    break;
  case INST_OPCODE_MOV:  
    operand = shift_operand(core, operand);
    arm_inst_mov_handler(core, instr, operand);  
    break;
  case INST_OPCODE_CMP:  
    operand = shift_operand(core, operand);
    arm_inst_cmp_handler(core, instr, operand);  
    break; 
  case INST_OPCODE_ADD:  
    operand = shift_operand(core, operand);
    arm_inst_add_handler(core, instr, operand);  
    break;  
  case INST_OPCODE_SUB:  
    operand = shift_operand(core, operand);
    arm_inst_sub_handler(core, instr, operand);  
    break;  
  default: // unknown instruction
    printf("unknown DPR instruction (0x%x)\n", *inst);
  }
}

// handler for data-processing instructions with immediates
static void arm_inst_DPI_handler(ARM_CORE *core, unsigned int *inst) {
  ARM_INST_DPI *instr = (ARM_INST_DPI *) inst;
  unsigned int operand = instr->operand2;
  operand = shift_ROR(INST_IMMEDIATE_VALUE(operand), INST_IMMEDIATE_SHCNT(operand));
  switch(instr->opcode) {
  case INST_OPCODE_MOV:  arm_inst_mov_handler(core, (ARM_INST_DPR *) instr, operand);  break;
  case INST_OPCODE_CMP:  arm_inst_cmp_handler(core, (ARM_INST_DPR *) instr, operand);  break; 
  case INST_OPCODE_ADD:  arm_inst_add_handler(core, (ARM_INST_DPR *) instr, operand);  break;  
  case INST_OPCODE_SUB:  arm_inst_sub_handler(core, (ARM_INST_DPR *) instr, operand);  break;  
  default: // unknown instruction
    printf("unknown DPI instruction (0x%x)\n", *inst);
  }
} 

// handler for single-data-transfer instructions with immediates
static void arm_inst_SDI_handler(ARM_CORE *core, unsigned int *inst) {
  ARM_INST_SDI *instr = (ARM_INST_SDI *) inst;

  switch(instr->Lbit) {
  case INST_OPCODE_STORE:  arm_inst_store_handler(core, instr);  break;
  case INST_OPCODE_LOAD:  arm_inst_load_handler(core, instr);  break;
  }
}

// handler for single-data-transfer instructions
static void arm_inst_SDT_handler(ARM_CORE *core, unsigned int *inst) {
  printf("NOT IMPLEMENTED SDT instruction (0x%x)\n", *inst);
  exit(1);
}

// handler for block-data-transfer instructions
static void arm_inst_BDT_handler(ARM_CORE *core, unsigned int *inst) {
  ARM_INST_BDT *instr = (ARM_INST_BDT *) inst;

  switch(instr->opcode) {
  case INST_OPCODE_PUSH:  
    if (instr->Rn == 13)  arm_inst_push_handler(core, instr);  
    else  printf("unknown BLOCK PUSH instruction (0x%x)\n", *inst);
    break;
  case INST_OPCODE_POP:  
    if (instr->Rn == 13)  arm_inst_pop_handler(core, instr);  
    else  printf("unknown BLOCK POP instruction (0x%x)\n", *inst);
    break;
  default: // unknown instruction
    printf("unknown BDT instruction (0x%x)\n", *inst);
  }
}

// handler for branch instructions
static void arm_inst_BRN_handler(ARM_CORE *core, unsigned int *inst) {
  ARM_INST_BRN *instr = (ARM_INST_BRN *) inst;
  long long offset = (instr->imm24 << 8) >> 6; // imm24 should be sign extended to 32 bits
  unsigned int exec = is_cond_met(core->reg.cpsr, instr->cond);

  if (glob_verbose)  {
    if (exec)  printf("[%d]EXEC: ", core->reg.inst_count);
    else        printf("[%d]SKIP: ", core->reg.inst_count);
    printf("  %x:  ", core->reg.__PC__ - 8);
    if (instr->Lbit)  printf("bl%s  %x\n", get_suffix(instr->cond), (unsigned int) (core->reg.__PC__ + offset));
    else                 printf("b%s   %x\n", get_suffix(instr->cond), (unsigned int) (core->reg.__PC__ + offset));
  }

  if (!exec) return;

  if (instr->Lbit) core->reg.__LR__ = core->reg.__PC__ - 4;
  core->reg.__PC__ += offset;
  core->reg.branch_taken = 1;
}

///////////////////////////////////////////////////////////////////////////
// Core management functions
///////////////////////////////////////////////////////////////////////////
static int fetch_inst(ARM_CORE *core) {
  unsigned int *inst_addr;

  if (core->reg.__PC__ == PROG_INITIAL_LR) return 0;

  if (core->reg.branch_taken)  {
    core->reg.branch_taken = 0;
    core->reg.__PC__ += 4;
  }

  inst_addr = (unsigned int *) prog_addr_to_host_addr(core->reg.__PC__ - 4, core->mm);
  if (*inst_addr == TRAP_INST_CODE) {
    glob_interactive = 1;
    reset_breakpoint(current[0]);
  }

  core->reg.curr_inst = *inst_addr;
  core->reg.__PC__ += 4;
  core->reg.inst_count++;

  return 1;
}

static void print_core(ARM_CORE *core) {
  int i, n = 16;

  for (i = 0; i < n; i++) {
    printf("_r[%2d] = 0x%08x   ", i, core->reg.regs[i]);
    if (((i+1) % 4) == 0) printf("\n");
  }
}

static void print_data(TASK_STRUCT *task) {
  int i;
  if (task->watch_point == 0 || task->watch_size == 0) return;

  unsigned int *host_addr = (unsigned int *) prog_addr_to_host_addr(task->watch_point, &task->mm);
  unsigned int newline_printed = 1;
  for (i = 0; i < task->watch_size; i++) {
    newline_printed = 0;
    printf("_W[%2d] = 0x%08x   ", i, host_addr[i]);
    if (((i+1) % 4) == 0) {
      printf("\n");
      newline_printed = 1;
    }
  }
  if (!newline_printed) printf("\n");
}

static void print_stack(ARM_CORE *core) {
  int i;
  int n = (PROG_INITIAL_SP - core->reg.__SP__)/4;
  n = (n > 16 ? 16 : n);
  unsigned int *host_addr = (unsigned int *) prog_addr_to_host_addr(core->reg.__SP__, core->mm);
  unsigned int newline_printed = 1;
  for (i = 0; i < n; i++) {
    newline_printed = 0;
    printf("SP[%2d] = 0x%08x   ", i, host_addr[i]);
    if (((i+1) % 4) == 0) {
      printf("\n");
      newline_printed = 1;
    }
  }
  if (!newline_printed) printf("\n");
}

void load_program(MM_STRUCT *mm, char *exe_file) {
  int fd;
  struct stat sbuf;

  printf("-------------------------------------------------------------------------------------\n");
  printf("Program loaded...\n\n");

  strcpy(mm->exe_file, exe_file);
  if ((fd = open(mm->exe_file, O_RDONLY)) < 0)  quit("open");
  if (fstat(fd, &sbuf) < 0) quit("fstat");
#if 0
  char buff[PAGE_SIZE];
  if (read(fd, buff, sizeof(buff)) <= 0) 
    quit("read");
  // Reading the first page is not enough to cover the section headers, so
  // the following call will crash.
  read_elf((long long unsigned int *) buff, mm);
#endif
  unsigned char *elf_hdr_base;
  if ((elf_hdr_base = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0)) 
      == MAP_FAILED) quit("mmap");
  read_elf((long long unsigned int *) elf_hdr_base, mm);
  munmap(elf_hdr_base, sbuf.st_size);

  if ((mm->text_base = mmap(0, mm->text_size, PROT_READ, MAP_PRIVATE, fd, 0)) 
      == MAP_FAILED) quit("mmap");

  unsigned char *data_base;
  unsigned int tail_size = PAGE_OFFSET(mm->data_offset);
  unsigned int data_size = mm->data_size + tail_size;
  mm->stack_iaddr = PROG_INITIAL_SP;
  mm->stack_size = PROG_STACK_SIZE;

  if ((data_base = mmap(0, data_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, PAGE_DOWN(mm->data_offset))) 
      == MAP_FAILED) quit("mmap");
  mm->data_base = data_base + tail_size;

  if ((mm->stack_base = mmap(NULL, PROG_STACK_SIZE, PROT_READ|PROT_WRITE, 
				       MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)) == MAP_FAILED) 
    quit("mmap");

  if (glob_debug) {
    printf("\nMM:\n");
    printf("[FILE]  ----------------  -----------------  size=0x%08x  -----------------------\n", 
	   (unsigned int) sbuf.st_size);
    printf("[TEXT]  vaddr=0x%08x  offset=0x%08x  size=0x%08x  64-vaddr=%p\n", 
	   mm->text_vaddr, mm->text_offset, mm->text_size, mm->text_base);
    printf("(DATA)  vaddr=0x%08x  offset=0x%08x  size=0x%08x  64-vaddr=%p\n", 
	   PAGE_DOWN(mm->data_vaddr), PAGE_DOWN(mm->data_offset), data_size, data_base);
    printf("[DATA]  vaddr=0x%08x  offset=0x%08x  size=0x%08x  64-vaddr=%p\n", 
	   mm->data_vaddr, mm->data_offset, mm->data_size, mm->data_base);
    printf("[STACK] vaddr=0x%08x  offset=0x%08x  size=0x%08x  64-vaddr=%p\n", 
	   mm->stack_iaddr, 0, mm->stack_size, mm->stack_base);
  }
  close(fd);
}

void init_core(ARM_CORE *core, TASK_STRUCT *task) {
  ARM_REG *reg = &task->reg;
  core->mm = &task->mm;
  core->reg.__SP__ = reg->__SP__;
  core->reg.__LR__ = reg->__LR__;
  core->reg.__PC__ = reg->__PC__;
  core->reg.cpsr.Z = 1;
  core->reg.cpsr.C = 1;
  core->reg.branch_taken = 1;
  //core->reg.inst_count = 0;
}

int run_core(ARM_CORE *core) {
  ARM_INST_COMMON *inst = (ARM_INST_COMMON *) &core->reg.curr_inst;
  void (*handler)(ARM_CORE *core, unsigned int *inst);
  char ch;

  printf("\nCORE:\n");
  print_core(core); 
  print_stack(core);
  print_data(current[0]);
  printf("-------------------------------------------------------------------------------------\n");

  while (fetch_inst(core)) {
    handler = arm_inst_type_handlers[inst->type];
    handler(core, (unsigned int *) inst);
    if (glob_debug) {
      print_core(core);
      print_stack(core);
      print_data(current[0]);
      printf("-------------------------------------------------------------------------------------\n");
    }
    if (glob_interactive) {
      if ((ch = getch()) == 'q') break;
    }
  }

  printf("Program terminated...\n");
  print_core(core);
  print_stack(core);
  print_data(current[0]);
  printf("-------------------------------------------------------------------------------------\n");
}

