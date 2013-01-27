#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "../util/alloc.h"
#include "../util/util.h"
#include "../util/dynarray.h"
#include "data_structs.h"
#include "sue.h"
#include "cc1.h"

static void sue_set_spel(struct_union_enum_st *sue, char *spel)
{
	if(!spel){
		int len = 6 + 6 + 3 + WHERE_BUF_SIZ + 1 + 1;
		spel = umalloc(len);
		snprintf(spel, len, "<anon %s @ %s>",
				sue_str(sue), where_str(&sue->where));
	}

	free(sue->spel);
	sue->spel = spel;
}

void enum_vals_add(sue_member ***pmembers, char *sp, expr *e)
{
	enum_member *emem = umalloc(sizeof *emem);
	sue_member *mem = umalloc(sizeof *mem);

	if(!e)
		e = (expr *)-1;

	emem->spel = sp;
	emem->val  = e;

	mem->enum_member = emem;

	dynarray_add((void ***)pmembers, mem);
}

int enum_nentries(struct_union_enum_st *e)
{
	int n = 0;
	sue_member **i;
	for(i = e->members; *i; i++, n++);
	return n;
}

int sue_size(struct_union_enum_st *st, const where *w)
{
	if(sue_incomplete(st))
		DIE_AT(w, "%s %s is incomplete", sue_str(st), st->spel);

	if(st->size)
		return st->size;

	if(st->primitive == type_enum)
		return st->size = type_primitive_size(type_int);

	ICE("%s of unfolded sue", __func__);
	return -1;
}

struct_union_enum_st *sue_find(symtable *stab, const char *spel)
{
	for(; stab; stab = stab->parent){
		struct_union_enum_st **i;

		for(i = stab->sues; i && *i; i++){
			struct_union_enum_st *st = *i;

			if(st->spel && !strcmp(st->spel, spel))
				return st;
		}
	}
	return NULL;
}

struct_union_enum_st *sue_add(symtable *const stab, char *spel, sue_member **members, enum type_primitive prim)
{
	struct_union_enum_st *sue;
	int new = 0;

	if(spel && (sue = sue_find(stab, spel))){
		char buf[WHERE_BUF_SIZ];

		snprintf(buf, sizeof buf, "%s", where_str(&sue->where));

		/* redef checks */
		if(sue->primitive != prim)
			DIE_AT(NULL, "trying to redefine %s as %s (from %s)",
					sue_str(sue),
					type_primitive_to_str(prim),
					buf);

		if(members && !sue_incomplete(sue))
			DIE_AT(NULL, "can't redefine %s %s's members (defined at %s)",
					sue_str(sue), sue->spel, buf);

	}else{
		sue = umalloc(sizeof *sue);
		sue->primitive = prim;

		new = 1;

		where_new(&sue->where);
	}

	if(members){
		if(prim == type_enum){
			/* check for duplicates */
			int i;

			for(i = 0; members[i]; i++){
				const char *const spel = members[i]->enum_member->spel;

				struct_union_enum_st *e_sue;
				enum_member *e_mem;

				enum_member_search(&e_mem, &e_sue, stab, spel);

				if(e_mem){
					char buf[WHERE_BUF_SIZ];
					DIE_AT(NULL, "redeclaration of enumerator %s (from %s)", spel, where_str_r(buf, &e_sue->where));
				}
			}

		}else{
			int i;

			for(i = 0; members[i]; i++){
				decl *d = members[i]->struct_member;
				int j;

				if(d->init)
					DIE_AT(&d->where, "%s member %s is initialised", sue_str(sue), d->spel);

				for(j = i + 1; members[j]; j++){
					if(!strcmp(d->spel, members[j]->struct_member->spel)){
						char buf[WHERE_BUF_SIZ];
						DIE_AT(&d->where, "duplicate member %s (from %s)",
								d->spel, where_str_r(buf, &members[j]->struct_member->where));
					}
				}
			}
		}
	}

	sue->anon = !spel;

	sue_set_spel(sue, spel);

	if(members){
		UCC_ASSERT(!sue->members, "redef of struct/union should've been caught");
		sue->members = members;
	}

	if(new)
		dynarray_add((void ***)&stab->sues, sue);

	return sue;
}

sue_member *sue_member_find(struct_union_enum_st *sue, const char *spel, where *die_where)
{
	sue_member **mi;

	for(mi = sue->members; mi && *mi; mi++){
		char *sp;

		if(sue->primitive == type_enum){
			enum_member *m = (*mi)->enum_member;
			sp = m->spel;
		}else{
			decl *d = (*mi)->struct_member;
			sp = d->spel;
		}

		if(!strcmp(spel, sp))
			return *mi;
	}

	if(die_where)
		DIE_AT(die_where, "%s %s has no member named \"%s\"", sue_str(sue), sue->spel, spel);

	return NULL;
}


void enum_member_search(enum_member **pm, struct_union_enum_st **psue, symtable *stab, const char *spel)
{
	for(; stab; stab = stab->parent){
		struct_union_enum_st **i;

		for(i = stab->sues; i && *i; i++){
			struct_union_enum_st *e = *i;

			if(e->primitive == type_enum){
				sue_member *smemb = sue_member_find(e, spel, NULL);

				if(smemb){
					*pm = smemb->enum_member;
					*psue = e;
					return;
				}
			}
		}
	}

	*pm = NULL;
	*psue = NULL;
}

decl *struct_union_member_find(struct_union_enum_st *sue, const char *spel, where *die_where)
{
	return sue_member_find(sue, spel, die_where)->struct_member;
}

decl *struct_union_member_at_idx(struct_union_enum_st *sue, int idx)
{
	int n = sue_nmembers(sue);
	/*
	 * not needed since there are checks in decl init code,
	 * but in case this is used elsewhere...
	 */
	if(idx >= n)
		return NULL;

	return sue->members[idx]->struct_member;
}

int struct_union_member_idx(struct_union_enum_st *sue, decl *member)
{
	int i;
	for(i = 0; sue->members[i]; i++)
		if(sue->members[i]->struct_member == member)
			return i;
	return -1;
}
