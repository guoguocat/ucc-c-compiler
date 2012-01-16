#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../util/util.h"
#include "tree.h"
#include "../util/alloc.h"
#include "typedef.h"

decl *typedef_find(struct tdeftable *defs, const char *spel)
{
	tdef *td;

	UCC_ASSERT(spel, "NULL spell in %s", __func__);

	for(; defs; defs = defs->parent)
		for(td = defs->first; td; td = td->next)
			if(!strcmp(td->decl->spel, spel))
				return td->decl;

	return NULL;
}

void typedef_add(struct tdeftable *defs, decl *d)
{
	tdef *new = umalloc(sizeof *new);

	new->decl = d;

	if(defs->first){
		tdef *td;

		for(td = defs->first; td->next; td = td->next);

		td->next = new;
	}else{
		defs->first = new;
	}
}
