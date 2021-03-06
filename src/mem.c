/* 
 * mem.c -- handles:
 *   memory allocation and deallocation
 *   keeping track of what memory is being used by whom
 * 
 * dprintf'ized, 15nov1995
 * 
 * $Id: mem.c,v 1.12 2000/01/08 21:23:14 per Exp $
 */
/* 
 * Copyright (C) 1997  Robey Pointer
 * Copyright (C) 1999, 2000  Eggheads
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define LOG_MISC 32
#define MEMTBLSIZE 25000	/* yikes! */

#if HAVE_CONFIG_H
#  include <config.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef int (*Function) ();
#include "mod/modvals.h"

extern module_entry *module_list;
void fatal(char *, int);

#ifdef DEBUG_MEM
unsigned long memused = 0;
static int lastused = 0;

struct {
  void *ptr;
  int size;
  short line;
  char file[20];
} memtbl[MEMTBLSIZE];

#endif

#ifdef HAVE_DPRINTF
#define dprintf dprintf_eggdrop
#endif

#define DP_HELP         0x7FF4

/* prototypes */
#if !defined(HAVE_PRE7_5_TCL) && defined(__STDC__)
void dprintf(int arg1, ...);
void putlog(int arg1, ...);
#else
void dprintf();
void putlog();
#endif
int expected_memory();
int expmem_chanprog();
int expmem_misc();
int expmem_fileq();
int expmem_users();
int expmem_dccutil();
int expmem_botnet();
int expmem_tcl();
int expmem_tclhash();
int expmem_net();
int expmem_modules();
int expmem_language();
int expmem_tcldcc();
void tell_netdebug();
void do_module_report(int, int, char *);

/* initialize the memory structure */
void init_mem()
{
#ifdef DEBUG_MEM
  int i;

  for (i = 0; i < MEMTBLSIZE; i++)
    memtbl[i].ptr = NULL;
#endif
}

/* tell someone the gory memory details */
void tell_mem_status(char *nick)
{
#ifdef DEBUG_MEM
  float per;

  per = ((lastused * 1.0) / (MEMTBLSIZE * 1.0)) * 100.0;
  dprintf(DP_HELP, "NOTICE %s :Memory table usage: %d/%d (%.1f%% full)\n",
	  nick, lastused, MEMTBLSIZE, per);
#endif
  dprintf(DP_HELP, "NOTICE %s :Think I'm using about %dk.\n", nick,
	  (int) (expected_memory() / 1024));
}

void tell_mem_status_dcc(int idx)
{
#ifdef DEBUG_MEM
  int exp;
  float per;

  exp = expected_memory();	/* in main.c ? */
  per = ((lastused * 1.0) / (MEMTBLSIZE * 1.0)) * 100.0;
  dprintf(idx, "Memory table: %d/%d (%.1f%% full)\n", lastused, MEMTBLSIZE, per);
  per = ((exp * 1.0) / (memused * 1.0)) * 100.0;
  if (per != 100.0)
    dprintf(idx, "Memory fault: only accounting for %d/%ld (%.1f%%)\n",
	    exp, memused, per);
  dprintf(idx, "Memory table itself occupies an additional %dk static\n",
	  (int) (sizeof(memtbl) / 1024));
#endif
}

void debug_mem_to_dcc(int idx)
{
#ifdef DEBUG_MEM
#define MAX_MEM 11
  unsigned long exp[MAX_MEM], use[MAX_MEM], l;
  int i, j;
  char fn[20], sofar[81];
  module_entry *me;
  char *p;

  exp[0] = expmem_language();
  exp[1] = expmem_chanprog();
  exp[2] = expmem_misc();
  exp[3] = expmem_users();
  exp[4] = expmem_net();
  exp[5] = expmem_dccutil();
  exp[6] = expmem_botnet();
  exp[7] = expmem_tcl();
  exp[8] = expmem_tclhash();
  exp[9] = expmem_modules(1);
  exp[10] = expmem_tcldcc();
  for (me = module_list; me; me = me->next)
    me->mem_work = 0;
  for (i = 0; i < MAX_MEM; i++)
    use[i] = 0;
  for (i = 0; i < lastused; i++) {
    strcpy(fn, memtbl[i].file);
    p = strchr(fn, ':');
    if (p)
      *p = 0;
    l = memtbl[i].size;
    if (!strcasecmp(fn, "language.c"))
      use[0] += l;
    else if (!strcasecmp(fn, "chanprog.c"))
      use[1] += l;
    else if (!strcasecmp(fn, "misc.c"))
      use[2] += l;
    else if (!strcasecmp(fn, "userrec.c"))
      use[3] += l;
    else if (!strcasecmp(fn, "net.c"))
      use[4] += l;
    else if (!strcasecmp(fn, "dccutil.c"))
      use[5] += l;
    else if (!strcasecmp(fn, "botnet.c"))
      use[6] += l;
    else if (!strcasecmp(fn, "tcl.c"))
      use[7] += l;
    else if (!strcasecmp(fn, "tclhash.c"))
      use[8] += l;
    else if (!strcasecmp(fn, "modules.c"))
      use[9] += l;
    else if (!strcasecmp(fn, "tcldcc.c"))
      use[10] += l;
    else if (p) {
      for (me = module_list; me; me = me->next)
	if (!strcmp(fn, me->name))
	  me->mem_work += l;
    } else {
      dprintf(idx, "Not logging file %s!\n", fn);
    }
    if (p)
      *p = ':';
  }
  for (i = 0; i < MAX_MEM; i++) {
    switch (i) {
    case 0:
      strcpy(fn, "language.c");
      break;
    case 1:
      strcpy(fn, "chanprog.c");
      break;
    case 2:
      strcpy(fn, "misc.c");
      break;
    case 3:
      strcpy(fn, "userrec.c");
      break;
    case 4:
      strcpy(fn, "net.c");
      break;
    case 5:
      strcpy(fn, "dccutil.c");
      break;
    case 6:
      strcpy(fn, "botnet.c");
      break;
    case 7:
      strcpy(fn, "tcl.c");
      break;
    case 8:
      strcpy(fn, "tclhash.c");
      break;
    case 9:
      strcpy(fn, "modules.c");
      break;
    case 10:
      strcpy(fn, "tcldcc.c");
      break;
    }
    if (use[i] == exp[i]) {
      dprintf(idx, "File '%-10s' accounted for %lu/%lu (ok)\n", fn, exp[i],
	      use[i]);
    } else {
      dprintf(idx, "File '%-10s' accounted for %lu/%lu (debug follows:)\n",
	      fn, exp[i], use[i]);
      strcpy(sofar, "   ");
      for (j = 0; j < lastused; j++) {
	if ((p = strchr(memtbl[j].file, ':')))
	  *p = 0;
	if (!strcasecmp(memtbl[j].file, fn)) {
	  if (p)
	    sprintf(&sofar[strlen(sofar)], "%-10s/%-4d:(%04d) ",
		    p + 1, memtbl[j].line, memtbl[j].size);
	  else
	    sprintf(&sofar[strlen(sofar)], "%-4d:(%04d) ",
		    memtbl[j].line, memtbl[j].size);

	  if (strlen(sofar) > 60) {
	    sofar[strlen(sofar) - 1] = 0;
	    dprintf(idx, "%s\n", sofar);
	    strcpy(sofar, "   ");
	  }
	}
	if (p)
	  *p = ':';
      }
      if (sofar[0]) {
	sofar[strlen(sofar) - 1] = 0;
	dprintf(idx, "%s\n", sofar);
      }
    }
  }
  for (me = module_list; me; me = me->next) {
    Function *f = me->funcs;
    int expt = 0;

    if ((f != NULL) && (f[MODCALL_EXPMEM] != NULL))
      expt = f[MODCALL_EXPMEM] ();
    if (me->mem_work == expt) {
      dprintf(idx, "Module '%-10s' accounted for %lu/%lu (ok)\n", me->name,
	      expt, me->mem_work);
    } else {
      dprintf(idx, "Module '%-10s' accounted for %lu/%lu (debug follows:)\n",
	      me->name, expt, me->mem_work);
      strcpy(sofar, "   ");
      for (j = 0; j < lastused; j++) {
	strcpy(fn, memtbl[j].file);
	if ((p = strchr(fn, ':')) != NULL) {
	  *p = 0;
	  if (!strcasecmp(fn, me->name)) {
	    sprintf(&sofar[strlen(sofar)], "%-10s/%-4d:(%04X) ", p + 1,
		    memtbl[j].line, memtbl[j].size);
	    if (strlen(sofar) > 60) {
	      sofar[strlen(sofar) - 1] = 0;
	      dprintf(idx, "%s\n", sofar);
	      strcpy(sofar, "   ");
	    }
	    *p = ':';
	  }
	}
      }
      if (sofar[0]) {
	sofar[strlen(sofar) - 1] = 0;
	dprintf(idx, "%s\n", sofar);
      }
    }
  }
  dprintf(idx, "--- End of debug memory list.\n");
#else
  dprintf(idx, "Compiled without extensive memory debugging (sorry).\n");
#endif
  tell_netdebug(idx);
}

void *n_malloc(int size, char *file, int line)
{
  void *x;
#ifdef DEBUG_MEM
  int i = 0;
#endif

  x = (void *) malloc(size);
  if (x == NULL) {
    putlog(LOG_MISC, "*", "*** FAILED MALLOC %s (%d) (%d): %s", file, line,
	   size, strerror(errno));
    fatal("Memory allocation failed", 0);
  }
#ifdef DEBUG_MEM
  if (lastused == MEMTBLSIZE) {
    putlog(LOG_MISC, "*", "*** MEMORY TABLE FULL: %s (%d)", file, line);
    return x;
  }
  i = lastused;
  memtbl[i].ptr = x;
  memtbl[i].line = line;
  memtbl[i].size = size;
  strncpy(memtbl[i].file, file, 19);
  memtbl[i].file[19] = 0;
  memused += size;
  lastused++;
#endif
  return x;
}

void *n_realloc(void *ptr, int size, char *file, int line)
{
  void *x;
  int i = 0;

  /* ptr == NULL is valid. Avoiding duplicate code further down */
  if (!ptr)
    return n_malloc(size, file, line);

  x = (void *) realloc(ptr, size);
  if (x == NULL) {
    i = i;
    putlog(LOG_MISC, "*", "*** FAILED REALLOC %s (%d)", file, line);
    return NULL;
  }
#ifdef DEBUG_MEM
  for (i = 0; (i < lastused) && (memtbl[i].ptr != ptr); i++);
  if (i == lastused) {
    putlog(LOG_MISC, "*", "*** ATTEMPTING TO REALLOC NON-MALLOC'D PTR: %s (%d)",
	   file, line);
    return NULL;
  }
  memused -= memtbl[i].size;
  memtbl[i].ptr = x;
  memtbl[i].line = line;
  memtbl[i].size = size;
  strncpy(memtbl[i].file, file, 19);
  memtbl[i].file[19] = 0;
  memused += size;
#endif
  return x;
}

void n_free(void *ptr, char *file, int line)
{
  int i = 0;

  if (ptr == NULL) {
    putlog(LOG_MISC, "*", "*** ATTEMPTING TO FREE NULL PTR: %s (%d)",
	   file, line);
    i = i;
    return;
  }
#ifdef DEBUG_MEM
  /* give tcl builtins an escape mechanism */
  if (line) {
    for (i = 0; (i < lastused) && (memtbl[i].ptr != ptr); i++);
    if (i == lastused) {
      putlog(LOG_MISC, "*", "*** ATTEMPTING TO FREE NON-MALLOC'D PTR: %s (%d)",
	     file, line);
      return;
    }
    memused -= memtbl[i].size;
    lastused--;
    memtbl[i].ptr = memtbl[lastused].ptr;
    memtbl[i].size = memtbl[lastused].size;
    memtbl[i].line = memtbl[lastused].line;
    strcpy(memtbl[i].file, memtbl[lastused].file);
  }
#endif
  free(ptr);
}
