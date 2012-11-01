#ifndef PARSE_TYPE_H
#define PARSE_TYPE_H

type_ref *parse_type(enum decl_storage *store);

/* (type) (ident) */
decl  *parse_decl_single(enum decl_mode mode);

/* (type) (ident) */
type_ref *parse_type_ref(void);

/* type ident(, ident, ...) */
decl **parse_decls_one_type(void);

/* type ident...; type ident...; */
decl **parse_decls_multi_type(enum decl_multi_mode mode);

funcargs *parse_func_arglist(void);

decl_init *parse_initialisation(void); /* expr or {{...}} */

#endif
