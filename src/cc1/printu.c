#include <stdio.h>
#include <stdarg.h>

#include "../util/util.h"
#include "../util/printu.h"

#include "data_structs.h"
#include "tree.h"
#include "decl.h"

/*
 * like printf but:
 * %Q - decl *
 * %R - type_ref *
 * %T - type *
 */

const struct printu printu_extras[] = {
	{ (printu_f *)decl_to_str,     'Q' },
	{ (printu_f *)type_ref_to_str, 'R' },
	{ (printu_f *)type_to_str,     'T' },
	{ NULL, 0 }
};
