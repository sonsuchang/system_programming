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

#ifndef __VCPU_H__
#define __VCPU_H__

#include "list.h"

#define MAX_STRING 128

///////////////////////////////////////////////////////////////////////////
// Definitions of Instruction types
///////////////////////////////////////////////////////////////////////////

typedef struct arm_inst_DPR_format { // format for arm data-processing instructions
  unsigned short Rm         :4;
  unsigned short subcode :4;
  unsigned short Rx          :4;
  unsigned short Rd          :4;
  unsigned short Rn          :4;
  unsigned short Sbit        :1;
  unsigned short opcode   :4;
  unsigned short type       :3;
  unsigned short cond      :4;
} ARM_INST_DPR;

typedef struct arm_inst_DPI_format { // format for arm data-processing instructions with immediates
  unsigned short operand2 :12;
  unsigned short Rd           :4;
  unsigned short Rn           :4;
  unsigned short Sbit         :1;
  unsigned short opcode    :4;
  unsigned short type        :3;
  unsigned short cond       :4;
} ARM_INST_DPI;

typedef  ARM_INST_DPI  ARM_INST_COMMON;

typedef struct arm_inst_SDI_format { // format for arm single-data-transfer instructions with immediates
  unsigned short imm12 :12;
  unsigned short Rt        :4;
  unsigned short Rn       :4;
  unsigned short Lbit      :1;
  unsigned short Wbit    :1;
  unsigned short Bbit     :1;
  unsigned short Ubit     :1;
  unsigned short Pbit     :1;
  unsigned short type   :3;
  unsigned short cond   :4;
} ARM_INST_SDI;

typedef struct arm_inst_BDT_format { // format for arm block-data-transfer instructions
    unsigned int regs      :16;
    unsigned int Rn        :4;
    unsigned int opcode :5;
    unsigned int type     :3;
    unsigned int cond    :4;
} ARM_INST_BDT;

typedef struct arm_inst_BRN_format { // format for arm branch instructions
    unsigned int imm24 :24;
    unsigned int Lbit    :1;
    unsigned int type   :3;
    unsigned int cond  :4;
} ARM_INST_BRN;

typedef struct arm_inst_BXW_format { // format for arm BXWritePC instructions
    unsigned int Rm         :4;
    unsigned int constant :24;
    unsigned int cond       :4;
} ARM_INST_BXW;

#define TRAP_INST_CODE  0xffffffff

///////////////////////////////////////////////////////////////////////////
// Definition of ARM CORE
///////////////////////////////////////////////////////////////////////////
typedef struct cpsr {
    unsigned short mode :5;
    unsigned short t      :1;
    unsigned short f      :1;
    unsigned short i      :1;
    unsigned short _f1 :16;
    unsigned short j      :1;
    unsigned short _f2  :2;
    unsigned short Q    :1;
    unsigned short V    :1;
    unsigned short C    :1;
    unsigned short Z    :1;
    unsigned short N    :1;
} CPSR;

#define PAGE_SHIFT  (12)
#define PAGE_SIZE  (1 << PAGE_SHIFT)
#define PAGE_MASK  (~(PAGE_SIZE-1))
#define PAGE_OFFSET(v)  ((v) & ((PAGE_SIZE)-1))
#define PAGE_DOWN(v)  ((v) & PAGE_MASK)
#define PAGE_ADDR(v)  ((void *) PAGE_DOWN(v))

typedef struct mm_struct {
  char exe_file[MAX_STRING];
  unsigned char *text_base;  // base address of the text segment
  unsigned char *data_base;  // base address of the data segment
  unsigned char *stack_base; // base address of the stack segment
  unsigned int text_vaddr;
  unsigned int text_offset;
  unsigned int text_size;
  unsigned int data_vaddr;
  unsigned int data_offset;
  unsigned int data_size;
  unsigned int stack_iaddr;
  unsigned int stack_size;
} MM_STRUCT;

typedef struct arm_reg {
#define __FP__  regs[11]
#define __SP__  regs[13]
#define __LR__  regs[14]
#define __PC__  regs[15]
  unsigned int regs[15];    // ARM registers
  CPSR  cpsr;                    // CPSR register
  unsigned int branch_taken; // is the branch taken in the previous instruction?
  unsigned int curr_inst;    // current instruction
  unsigned int inst_count; // number of instructions executed
} ARM_REG;

typedef struct arm_core {
  ARM_REG reg;
  MM_STRUCT *mm;
} ARM_CORE;

typedef struct task_struct {
  char comm[MAX_STRING];
  ARM_REG reg;
  MM_STRUCT mm;
  unsigned int break_point;
  unsigned int inst_at_bp;
  unsigned int watch_point;
  unsigned int watch_size;
  unsigned int cpu_mask;
  struct list_head task_list;
} TASK_STRUCT;

typedef struct vbd_config {
  // SCHED node
  char sched_policy[MAX_STRING];
  unsigned int timeslice;
  // TASKSET node
  struct list_head task_list;
} VBD_CONFIG;

extern ARM_CORE vcpu[4];
extern TASK_STRUCT *current[4];
extern int glob_verbose;
extern int glob_debug;
extern int glob_interactive;

extern void load_program(MM_STRUCT *mm, char *exe_file);
extern void init_core(ARM_CORE *core, TASK_STRUCT *task);
extern int run_core(ARM_CORE *core);

///////////////////////////////////////////////////////////////////////////
// Program-specific macros
///////////////////////////////////////////////////////////////////////////
#define PROG_INITIAL_LR  0x88888888
#define PROG_INITIAL_SP  0x80000000
#define PROG_STACK_SIZE 0x00100000
#define PROG_STACK_BASE (PROG_INITIAL_SP - PROG_STACK_SIZE)

#endif // __VCPU_H__
