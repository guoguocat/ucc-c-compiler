#include "ops.h"
#include "expr__Generic.h"
#include "../type_is.h"
#include "../type_nav.h"

const char *str_expr__Generic()
{
	return "_Generic";
}

static void generic_lea(expr *e)
{
	lea_expr(e->bits.generic.chosen->e);
}

void fold_expr__Generic(expr *e, symtable *stab)
{
	struct generic_lbl **i, *def;

	def = NULL;

	/* we use the non-decayed type */
	fold_expr_no_decay(e->expr, stab);

	for(i = e->bits.generic.list; i && *i; i++){
		struct generic_lbl **j, *l = *i;

		fold_expr_no_decay(l->e, stab);

		for(j = i + 1; *j; j++){
			type *m = (*j)->t;

			/* duplicate default checked below */
			if(m && (type_cmp(m, l->t, 0) & TYPE_EQUAL_ANY))
				die_at(&(*j)->e->where, "duplicate type in _Generic: %s",
						type_to_str(l->t));
		}


		if(l->t){
			enum { OKAY, INCOMPLETE, VARIABLE, FUNC } prob = OKAY;
			const char *sprob;

			fold_type(l->t, stab);

			if(!type_is_complete(l->t))
				prob = INCOMPLETE;
			else if(type_is_variably_modified(l->t))
				prob = VARIABLE;
			else if(type_is_func_or_block(l->t))
				prob = FUNC;

			switch(prob){
				case INCOMPLETE:
					sprob = "incomplete";
					break;
				case VARIABLE:
					sprob = "variably-modified";
					break;
				case FUNC:
					sprob = "function";
					break;
				case OKAY:
					sprob = NULL;
					break;
			}

			if(sprob){
				die_at(&l->e->where, "%s type '%s' in _Generic",
						sprob, type_to_str(l->t));
			}

			if(type_cmp(e->expr->tree_type, l->t, 0) & TYPE_EQUAL_ANY){
				UCC_ASSERT(!e->bits.generic.chosen,
						"already chosen expr for _Generic");
				e->bits.generic.chosen = l;
			}
		}else{
			if(def)
				die_at(&def->e->where, "second default for _Generic");
			def = l;
		}
	}


	if(!e->bits.generic.chosen){
		if(def)
			e->bits.generic.chosen = def;
		else
			die_at(&e->where, "no type satisfying %s", type_to_str(e->expr->tree_type));
	}

	if(expr_is_lval(e->bits.generic.chosen->e))
		e->f_lea = generic_lea;

	e->tree_type = e->bits.generic.chosen->e->tree_type;

	/* direct our location to the sub-expression */
	memcpy_safe(&e->where, &e->bits.generic.chosen->e->where);
}

void gen_expr__Generic(expr *e)
{
	gen_expr(e->bits.generic.chosen->e);
}

void gen_expr_str__Generic(expr *e)
{
	struct generic_lbl **i;

	idt_printf("_Generic expr:\n");
	gen_str_indent++;
	print_expr(e->expr);
	gen_str_indent--;

	idt_printf("_Generic choices:\n");
	gen_str_indent++;
	for(i = e->bits.generic.list; i && *i; i++){
		struct generic_lbl *l = *i;

		if(e->bits.generic.chosen == l)
			idt_printf("[Chosen]\n");

		if(l->t){
			idt_printf("type: ");
			gen_str_indent++;
			print_type(l->t, NULL);
			gen_str_indent--;
			fprintf(gen_file(), "\n");
		}else{
			idt_printf("default:\n");
		}
		idt_printf("expr:\n");
		gen_str_indent++;
		print_expr(l->e);
		gen_str_indent--;
	}
	gen_str_indent--;
}

static void const_expr__Generic(expr *e, consty *k)
{
	/* we're const if our chosen expr is */
	UCC_ASSERT(e->bits.generic.chosen, "_Generic const check before fold");

	const_fold(e->bits.generic.chosen->e, k);
}

void mutate_expr__Generic(expr *e)
{
	e->f_const_fold = const_expr__Generic;
}

expr *expr_new__Generic(expr *test, struct generic_lbl **lbls)
{
	expr *e = expr_new_wrapper(_Generic);
	e->expr = test;
	e->bits.generic.list = lbls;
	return e;
}

void gen_expr_style__Generic(expr *e)
{
	struct generic_lbl **i;

	stylef("_Generic(");
	gen_expr(e->expr);

	for(i = e->bits.generic.list; i && *i; i++){
		struct generic_lbl *l = *i;

		idt_printf("%s: ",
				l->t ? type_to_str(l->t) : "default");

		gen_expr(l->e);
	}
}
