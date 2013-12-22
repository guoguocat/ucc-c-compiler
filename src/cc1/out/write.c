#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "../../util/where.h"
#include "../../util/alloc.h"

#include "asm.h"
#include "write.h"
#include "dbg.h"
#include "lbl.h"

#include "../cc1.h" /* cc_out */
#include "../str.h" /* str_add_escape */

static void out_dbg_flush(void);


void out_asmv(
		enum section_type sec, enum p_opts opts,
		const char *fmt, va_list l)
{
	FILE *f = cc_out[sec];

	out_dbg_flush();

	if((opts & P_NO_INDENT) == 0)
		fputc('\t', f);

	vfprintf(f, fmt, l);

	if((opts & P_NO_NL) == 0)
		fputc('\n', f);
}

void out_asm(const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	out_asmv(SECTION_TEXT, 0, fmt, l);
	va_end(l);
}

void out_asm2(
		enum section_type sec, enum p_opts opts,
		const char *fmt, ...)
{
	va_list l;
	va_start(l, fmt);
	out_asmv(sec, opts, fmt, l);
	va_end(l);
}

/* -- dbg -- */

static struct file_idx
{
	const char *fname;
	struct file_idx *next;
} *file_head = NULL;

static where dbg_where;

static int last_file = -1, last_line = -1;

int dbg_add_file(const char *nam, int *new)
{
	struct file_idx **p;
	int i = 1; /* indexes start at 1 */

	if(new)
		*new = 0;

	for(p = &file_head; *p; p = &(*p)->next, i++)
		if(!strcmp(nam, (*p)->fname))
			return i;

	if(new)
		*new = 1;
	*p = umalloc(sizeof **p);
	(*p)->fname = nam;
	return i;
}

static void out_dbg_flush()
{
	/* .file <fileidx> "<name>"
	 * .loc <fileidx> <line> <col>
	 */
	int idx, new;

	if(!dbg_where.fname || !cc1_gdebug)
		return;

	idx = dbg_add_file(dbg_where.fname, &new);

	if(last_file == idx && last_line == dbg_where.line)
		return;
	/* XXX: prevents recursion as well as collapsing multiples */
	last_file = idx;
	last_line = dbg_where.line;

	/* TODO: escape w->fname */
	if(new){
		char *esc = str_add_escape(dbg_where.fname, strlen(dbg_where.fname));
		out_asm(".file %d \"%s\"", idx, esc);
		free(esc);
	}

	out_asm(".loc %d %d %d",
			idx,
			dbg_where.line,
			dbg_where.chr,
			dbg_where.fname);
}

void out_dbg_where(where *w)
{
	memcpy_safe(&dbg_where, w);
}
