#ifndef SYM_FOLD_H
#define SYM_FOLD_H

void symtab_fold_sues(symtable *stab);
void symtab_fold_decls(symtable *tab);

/* struct layout, check for duplicate decls */
void symtab_fold_decls_sues(symtable *);

unsigned symtab_layout_decls(
		symtable *, unsigned current);

#endif
