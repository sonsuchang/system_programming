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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>

#include "list.h"
#include "vcpu.h"


int glob_verbose = 1;
int glob_debug = 0;
int glob_interactive = 0;
char glob_config_file[MAX_STRING];
char glob_exec_file[MAX_STRING];
char glob_entry_point[MAX_STRING];
char glob_watch_point[MAX_STRING] = "0x0";
char glob_watch_size[MAX_STRING] = "0";

///////////////////////////////////////////////////////////////////////////
// Linux dependent code
///////////////////////////////////////////////////////////////////////////
static struct termios oldterm, newterm;

/* Initialize new terminal i/o settings */
static void init_termios(int echo) {
  tcgetattr(0, &oldterm); /* grab old terminal i/o settings */
  newterm = oldterm; /* make new settings same as old settings */
  newterm.c_lflag &= ~ICANON; /* disable buffered i/o */
  newterm.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
  tcsetattr(0, TCSANOW, &newterm); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
static void reset_termios(void) {
  tcsetattr(0, TCSANOW, &oldterm);
}

/* Read 1 character - echo defines echo mode */
static char getch_(int echo) {
  char ch;
  init_termios(echo);
  ch = getchar();
  reset_termios();
  return ch;
}

/* Read 1 character without echo */
char getch(void) {  return getch_(0); }

/* Read 1 character with echo */
char getche(void) {  return getch_(1); }

///////////////////////////////////////////////////////////////////////////
// Main code
///////////////////////////////////////////////////////////////////////////

void usage(char *progname) {
  printf("usage: %s [-d] [-i] [-f ARM_exec_file] [-e entry_addr] [-w watch_addr] [-n watch_size (in words)] \n", progname);
  printf("example: %s -d -f bsort -e 0x105dc -w 0x95088 -n 10\n", progname);
  printf("example: %s -d -f fact -e 0x10518 -w 0x96724 -n 1\n", progname);
}

int main(int argc,char *argv[]) {
  ARM_CORE *core = &vcpu[0];
  TASK_STRUCT *task;
  int opt;

  while ((opt = getopt(argc, argv, "c:de:f:in:w:")) != -1) {
    switch (opt) {
    case 'c': strcpy(glob_config_file, optarg); break;
    case 'd': glob_debug = 1; break;
    case 'i': glob_interactive = 1; break;
    case 'f': strcpy(glob_exec_file, optarg); break;
    case 'e': strcpy(glob_entry_point, optarg); break;
    case 'w': strcpy(glob_watch_point, optarg); break;
    case 'n': strcpy(glob_watch_size, optarg); break;
    default:
      printf("unexpected options!\n");
      usage(argv[0]);
      exit(1);
    }
  }

  if (optind > argc) { 
    printf("unknown arguments\n");
    usage(argv[0]);
    exit(1);
  }
 
  if (strlen(glob_config_file) == 0 && strlen(glob_exec_file) == 0) {
    printf("neither config file nor executable file\n");
    usage(argv[0]);
    exit(1);
  }

  if (strlen(glob_exec_file) > 0 && strlen(glob_entry_point) == 0) {
    printf("no entry address\n");
    usage(argv[0]);
    exit(1);
  }

  if (strlen(glob_config_file) > 0 && strlen(glob_entry_point) != 0) {
    printf("entry address ignored if a config file is given\n");
  }

  if (strlen(glob_exec_file) > 0) { // single task emulation
    if ((task = (TASK_STRUCT *) malloc(sizeof(TASK_STRUCT))) == NULL)
      quit("malloc");

    strcpy(task->comm, glob_exec_file);
    task->reg.__SP__ = 0x80000000;
    task->reg.__LR__ = 0x88888888;
    task->reg.__PC__ = strtoul(glob_entry_point, NULL, 16);
    task->watch_point = strtoul(glob_watch_point, NULL, 16);
    task->watch_size = atoi(glob_watch_size);

    printf("ARM executable file = %s\n", task->comm);
    printf("entry_point = 0x%x\n", task->reg.__PC__);
    printf("watch_point = 0x%x\n", task->watch_point);
    printf("watch_size = %d\n", task->watch_size);

    load_program(&task->mm, task->comm);
    current[0] = task;
    init_core(core, task);
    run_core(core);

    free(task);
  }
  else if (strlen(glob_config_file) > 0) { // multiple task emulation
    VBD_CONFIG config;
    struct list_head *task_list = &config.task_list;
    TASK_STRUCT *p, *n;
    INIT_LIST_HEAD(task_list);

    read_vbd_config(glob_config_file, &config);
    printf("sched_policy = %s\n", config.sched_policy);
    printf("timeslice = %d\n", config.timeslice);

    list_for_each_entry_safe(p, n, task_list, task_list) {
      if (p) {
	printf("ARM executable file = %s\n", p->comm);
	printf("SP = 0x%x\n", p->reg.__SP__);
	printf("LR = 0x%x\n", p->reg.__LR__);
	printf("PC = 0x%x\n", p->reg.__PC__);
	printf("break_point = 0x%x\n", p->break_point);
	printf("watch_point = 0x%x\n", p->watch_point);
	printf("watch_size = %d\n", p->watch_size);
	printf("cpu_mask = 0x%x\n", p->cpu_mask);

	getche();

	load_program(&p->mm, p->comm);

	if (p->break_point > 0)
	  set_breakpoint(p);

	current[0] = p;
	init_core(core, p);
	run_core(core);
      }
    }

    printf("inst_count = %d\n", core->reg.inst_count);
  }

  return 0;
}
