#ifndef TREE_H
#define TREE_H

typedef struct expr expr;
typedef struct stmt stmt;
typedef struct stmt_flow stmt_flow;

typedef struct sym         sym;
typedef struct symtable    symtable;
typedef struct symtable_global symtable_global;

typedef struct tdef        tdef;
typedef struct tdeftable   tdeftable;
typedef struct struct_union_enum_st struct_union_enum_st;

typedef struct decl        decl;
typedef struct funcargs    funcargs;
typedef struct decl_attr   decl_attr;

typedef struct decl_init   decl_init;

#include "type.h"

const type *type_new_primitive(enum type_primitive);
const type *type_new_primitive_sue(enum type_primitive, struct_union_enum_st *);
#define type_free(x) free(x)

void where_new(struct where *w);

const char *op_to_str(  const enum op_type o);
const char *type_to_str(const type *t);

const char *type_primitive_to_str(const enum type_primitive);
const char *type_qual_to_str(     const enum type_qualifier, int trailing_space);

int type_floating(enum type_primitive);
unsigned type_size( const type *, where *from);
unsigned type_align(const type *, where *from);
unsigned type_primitive_size(enum type_primitive tp);

int op_is_commutative(enum op_type o);
int op_is_relational(enum op_type o);
int op_is_shortcircuit(enum op_type o);
int op_is_comparison(enum op_type o);
int op_can_compound(enum op_type o);
int op_can_float(enum op_type o);


#define SPEC_STATIC_BUFSIZ      64
#define TYPE_STATIC_BUFSIZ      (SPEC_STATIC_BUFSIZ + 64)
#define TYPE_REF_STATIC_BUFSIZ  (TYPE_STATIC_BUFSIZ + 256)
#define DECL_STATIC_BUFSIZ      (TYPE_REF_STATIC_BUFSIZ + 16)


/* tables local to the current scope */
extern symtable *current_scope;
numeric *numeric_new(long v);

extern const where *eof_where;

#define EOF_WHERE(exp, code)                 \
	do{                                        \
		const where *const eof_save = eof_where; \
		eof_where = (exp);                       \
		{ code; }                                \
		eof_where = eof_save;                    \
	}while(0)

#endif
