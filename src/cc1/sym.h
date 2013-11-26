#ifndef SYM_H
#define SYM_H

struct sym
{
	int offset; /* stack offset / arg index */

	enum sym_type
	{
		sym_global, /* externs are sym_global */
		sym_local,
		sym_arg
	} type;

	decl *decl, *func;

	/* static analysis */
	int nreads, nwrites;
};

typedef struct static_assert static_assert;

struct static_assert
{
	expr *e;
	char *s;
	symtable *scope;
	int checked;
};

struct symtable
{
	int auto_total_size;
	unsigned folded : 1, laidout : 1;
	unsigned internal_nest : 1, are_params : 1;
	decl *in_func; /* for r/w checks on args and return-type checks */
	/*
	 * { int i; 5; int j; }
	 * j's symtab is internally represented like:
	 * { int i; 5; { int j; } }
	 *
	 * this marks if it is so, for duplicate checking
	 */

	symtable *parent, **children;

	struct_union_enum_st **sues;

	/* identifiers and typedefs */
	decl **decls;

	/* char * => label * */
	struct dynmap *labels;

	static_assert **static_asserts;
};

typedef struct symtable_gasm symtable_gasm;
struct symtable_gasm
{
	decl *before; /* the decl this occurs before - NULL if last */
	char *asm_str;
};

struct symtable_global
{
	symtable stab; /* ABI compatible with struct symtable */
	symtable_gasm **gasms;
	dynmap *literals;
};

sym *sym_new(decl *d, enum sym_type t);
sym *sym_new_stab(symtable *, decl *d, enum sym_type t);

symtable_global *symtabg_new(void);

symtable *symtab_new(symtable *parent);
void      symtab_set_parent(symtable *child, symtable *parent);
void      symtab_rm_parent( symtable *child);

void symtab_add_params(symtable *, decl **);

symtable *symtab_root(symtable *child);
symtable *symtab_func_root(symtable *stab);
#define symtab_func(st) symtab_func_root(st)->in_func
symtable_global *symtab_global(symtable *);

int symtab_nested_internal(symtable *parent, symtable *nest);

sym  *symtab_search(symtable *, const char *);
decl *symtab_search_d(symtable *, const char *, symtable **pin);
int   typedef_visible(symtable *stab, const char *spel);

const char *sym_to_str(enum sym_type);

#define sym_free(s) free(s)

/* labels */
struct label *symtab_label_find_or_new(symtable *, char *, where *);
void symtab_label_add(symtable *, struct label *);

#endif
