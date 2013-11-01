#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../../util/where.h"
#include "../../util/platform.h"
#include "../../util/util.h"
#include "../../util/dynarray.h"
#include "../../util/alloc.h"

#include "../str.h"

#include "../data_structs.h"
#include "../expr.h"
#include "../tree.h"
#include "../const.h"
#include "../funcargs.h"
#include "../sue.h"

#include "../cc1.h" /* cc_out[] */

#include "lbl.h"
#include "dbg.h"

struct dwarf_state
{
	struct dwarf_sec
	{
		FILE *f;
		int idx;
		int indent;
		unsigned length;
	} abbrev, info;
#define DWARF_SEC_INIT(fp) { (fp), 1, 1, 0 }
};

struct dwarf_block /* DW_FORM_block1 */
{
	unsigned len;
	unsigned *vals;
};

enum dwarf_block_ops
{
	DW_OP_plus_uconst = 0x23
};

enum dwarf_key
{
	DW_TAG_base_type = 0x24,
	DW_TAG_variable = 0x34,
	DW_TAG_typedef = 0x16,
	DW_TAG_pointer_type = 0xf,
	DW_TAG_array_type = 0x1,
	DW_TAG_subrange_type = 0x21,
	DW_TAG_const_type = 0x26,
	DW_TAG_subroutine_type = 0x15,
	DW_TAG_formal_parameter = 0x5,
	DW_TAG_enumeration_type = 0x4,
	DW_TAG_enumerator = 0x28,
	DW_TAG_structure_type = 0x13,
	DW_TAG_union_type = 0x17,
	DW_TAG_member = 0xd,
	DW_AT_data_member_location = 0x38,

	DW_AT_byte_size = 0xb,
	DW_AT_encoding = 0x3e,
	DW_AT_name = 0x3,
	DW_AT_language = 0x13,
	DW_AT_low_pc = 0x11,
	DW_AT_high_pc = 0x12,
	DW_AT_producer = 0x25,
	DW_AT_type = 0x49,
	DW_AT_sibling = 0x1,
	DW_AT_lower_bound = 0x22,
	DW_AT_upper_bound = 0x2f,
	DW_AT_prototyped = 0x27,
	DW_AT_location = 0x2,
	DW_AT_const_value = 0x1c,
	DW_AT_accessibility = 0x32,
	DW_ACCESS_public = 0x01,
};
enum dwarf_valty
{
	DW_FORM_addr = 0x1,
	DW_FORM_data1 = 0xb,
	DW_FORM_data2 = 0x5,
	DW_FORM_string = 0x8,
	/*
	DW_FORM_ref1 = 0x11,
	DW_FORM_ref2 = 0x12,
	DW_FORM_ref8 = 0x14,
	*/
	DW_FORM_ref4 = 0x13,
	DW_FORM_flag = 0xc,
	DW_FORM_block1 = 0xa,
};
enum
{
	DW_TAG_compile_unit = 0x11,
	DW_CHILDREN_no = 0,
	DW_CHILDREN_yes = 1,

	DW_ATE_boolean = 0x2,
	DW_ATE_float = 0x4,
	DW_ATE_signed = 0x5,
	DW_ATE_signed_char = 0x6,
	DW_ATE_unsigned = 0x7,
	DW_ATE_unsigned_char = 0x8,

	DW_LANG_C89 = 0x1,
	DW_LANG_C99 = 0xc
};

static void indent(FILE *f, int idt)
{
	while(idt --> 0)
		fputc('\t', f);
}

static void dwarf_smallest(unsigned long val, const char **pty, unsigned *int_sz)
{
	if((unsigned char)val == val)
		*pty = "byte", *int_sz = 1;
	else if((unsigned short)val == val)
		*pty = "word", *int_sz = 2;
	else if((unsigned)val == val)
		*pty = "long", *int_sz = 4;
	else
		*pty = "quad", *int_sz = 8;
}

#define VAL_TERM -1L
static void dwarf_sec_val(struct dwarf_sec *sec, long val, ...) /* -1 terminator */
{
	va_list l;

	va_start(l, val);

	do{
		const char *ty;
		unsigned int_sz;

		dwarf_smallest(val, &ty, &int_sz);

		indent(sec->f, sec->indent);
		fprintf(sec->f, ".%s %ld\n", ty, val);
		sec->length += int_sz;

		val = va_arg(l, long);
	}while(val != VAL_TERM);

	va_end(l);
}

static void dwarf_attr(
		struct dwarf_state *st,
		enum dwarf_key key, enum dwarf_valty val,
		...)
{
	va_list l;

	/* abbrev part */
	dwarf_sec_val(&st->abbrev, key, val, VAL_TERM);

	indent(st->info.f, st->info.indent);

	/* info part */
	va_start(l, val);
	switch(val){
		case DW_FORM_block1:
		{
			const struct dwarf_block *blk = va_arg(l, struct dwarf_block *);
			unsigned i;

			fprintf(st->info.f, ".byte %d", (signed char)blk->len);

			for(i = 0; i < blk->len; i++)
				fprintf(st->info.f, ", %d", blk->vals[i]);

			fputc('\n', st->info.f);

			st->info.length += 1 + blk->len;
			break;
		}
		case DW_FORM_ref4:
			fprintf(st->info.f, ".long %u", va_arg(l, unsigned));
			st->info.length += 4;
			break;
		case DW_FORM_addr:
			fprintf(st->info.f, ".quad 0x%lx", (long)va_arg(l, void *));
			st->info.length += 8;
			break;
		case DW_FORM_data1:
		case DW_FORM_flag:
			fprintf(st->info.f, ".byte %d", (signed char)va_arg(l, int));
			st->info.length++;
			break;
		case DW_FORM_data2:
			fprintf(st->info.f, ".word %d", (short)va_arg(l, int));
			st->info.length += 2;
			break;
		case DW_FORM_string:
		{
			char *esc = va_arg(l, char *);
			esc = str_add_escape(esc, strlen(esc));
			fprintf(st->info.f, ".asciz \"%s\"", esc);
			st->info.length += strlen(esc) + 1;
			free(esc);
			break;
		}
	}
	va_end(l);

	fputc('\n', st->info.f);
}

static void dwarf_sec_start(struct dwarf_sec *sec)
{
	dwarf_sec_val(sec, sec->idx++, VAL_TERM);
	sec->indent++;
}

static void dwarf_sec_end(struct dwarf_sec *sec)
{
	sec->indent--;
	dwarf_sec_val(sec, 0, VAL_TERM);
}

static void dwarf_start(struct dwarf_state *st)
{
	dwarf_sec_start(&st->abbrev);
	dwarf_sec_start(&st->info);
}

static void dwarf_end(struct dwarf_state *st)
{
	dwarf_sec_end(&st->abbrev);
	st->info.indent--; /* we don't terminate info entries */
}

static void dwarf_abbrev_start(struct dwarf_state *st, int b1, int b2)
{
	dwarf_sec_val(&st->abbrev, b1, b2, VAL_TERM);
	st->abbrev.indent++;
}

static void dwarf_basetype(struct dwarf_state *st, enum type_primitive prim, int enc)
{
	dwarf_start(st); {
		dwarf_abbrev_start(st, DW_TAG_base_type, DW_CHILDREN_no); {
			dwarf_attr(st, DW_AT_name,      DW_FORM_string, type_primitive_to_str(prim));
			dwarf_attr(st, DW_AT_byte_size, DW_FORM_data1,  type_primitive_size(prim));
			dwarf_attr(st, DW_AT_encoding,  DW_FORM_data1,  enc);
		} dwarf_sec_end(&st->abbrev);
	} dwarf_end(st);
}

static void dwarf_sue_header(
		struct dwarf_state *st, struct_union_enum_st *sue,
		int dwarf_tag, int children)
{
	dwarf_start(st); {
		dwarf_abbrev_start(st, dwarf_tag, children ? DW_CHILDREN_yes : DW_CHILDREN_no); {
			/*dwarf_attr(st, DW_AT_sibling, ... next?);*/
			if(!sue->anon)
				dwarf_attr(st, DW_AT_name, DW_FORM_string, sue->spel);
			if(sue_complete(sue))
				dwarf_attr(st, DW_AT_byte_size, DW_FORM_data1, sue_size(sue, NULL));
		} dwarf_sec_end(&st->abbrev);
	} dwarf_end(st);
}

static unsigned dwarf_type(struct dwarf_state *st, type_ref *ty)
{
	unsigned this_start;

	switch(ty->type){
		case type_ref_type:
		{
			struct_union_enum_st *sue = ty->bits.type->sue;

			this_start = st->info.length;

			if(sue){
				switch(sue->primitive){
					default:
						ucc_unreach(0);

					case type_enum:
					{
						sue_member **i;

						dwarf_sue_header(st, sue, DW_TAG_enumeration_type, /*children:*/0);

						ICW_1("need to make enumerators siblings of the enumeration");

						/* enumerators */
						for(i = sue->members; i && *i; i++){
							dwarf_start(st); {
								dwarf_abbrev_start(st, DW_TAG_enumerator, DW_CHILDREN_no); {
									enum_member *emem = (*i)->enum_member;

									dwarf_attr(st,
											DW_AT_name, DW_FORM_string,
											emem->spel);

									dwarf_attr(st,
											DW_AT_const_value, DW_FORM_data1,
											(int)const_fold_val(emem->val));

								} dwarf_sec_end(&st->abbrev);
							} dwarf_end(st);
						}
						break;
					}

					case type_union:
					case type_struct:
					{
						const size_t nmem = dynarray_count(sue->members);
						sue_member **si;
						unsigned i;
						unsigned *mem_offsets = nmem ? umalloc(nmem * sizeof *mem_offsets) : NULL;

						ICW_1("need to make members siblings of the struct/union");

						/* member types */
						for(i = 0; i < nmem; i++)
							mem_offsets[i] = dwarf_type(st, sue->members[i]->struct_member->ref);

						/* must update since we might've output extra type information */
						this_start = st->info.length;

						dwarf_sue_header(
								st, sue,
								sue->primitive == type_struct
									? DW_TAG_structure_type
									: DW_TAG_union_type,
								/*children:*/0);

						/* members */
						for(i = 0, si = sue->members; i < nmem; i++, si++){
							dwarf_start(st); {
								dwarf_abbrev_start(st, DW_TAG_member, DW_CHILDREN_no); {
									struct dwarf_block offset;
									unsigned offset_data[2];

									decl *dmem = (*si)->struct_member;

									dwarf_attr(st,
											DW_AT_name, DW_FORM_string,
											dmem->spel);

									dwarf_attr(st,
											DW_AT_type, DW_FORM_ref4,
											mem_offsets[i]);

									/* TODO: bitfields */
									offset_data[0] = DW_OP_plus_uconst;
									offset_data[1] = dmem->struct_offset;

									offset.len = 2;
									offset.vals = offset_data;

									dwarf_attr(st,
											DW_AT_data_member_location, DW_FORM_block1,
											&offset);

									dwarf_attr(st,
											DW_AT_accessibility, DW_FORM_flag,
											DW_ACCESS_public);

								} dwarf_sec_end(&st->abbrev);
							} dwarf_end(st);
						}

						free(mem_offsets);
						break;
					}
				}

			}else{
				/* TODO: unsigned type */
				dwarf_basetype(st, ty->bits.type->primitive,
						type_ref_is_floating(ty) ? DW_ATE_float : DW_ATE_signed);
			}
			break;
		}

		case type_ref_tdef:
			if(ty->bits.tdef.decl){
				decl *d = ty->bits.tdef.decl;
				const unsigned sub_pos = dwarf_type(st, d->ref);

				this_start = st->info.length;

				dwarf_start(st); {
					dwarf_abbrev_start(st, DW_TAG_typedef, DW_CHILDREN_yes); {
						dwarf_attr(st, DW_AT_name, DW_FORM_string, d->spel);
						dwarf_attr(st, DW_AT_type, DW_FORM_ref4, sub_pos);
					} dwarf_sec_end(&st->abbrev);
				} dwarf_end(st);
			}else{
				/* skip typeof() */
				this_start = st->info.length;
				dwarf_type(st, ty->bits.tdef.type_of->tree_type);
			}
			break;

		case type_ref_ptr:
		{
			const unsigned sub_pos = dwarf_type(st, ty->ref);

			this_start = st->info.length;

			dwarf_start(st); {
				dwarf_abbrev_start(st, DW_TAG_pointer_type, DW_CHILDREN_yes); {
					dwarf_attr(st, DW_AT_byte_size, DW_FORM_data1, platform_word_size());
					dwarf_attr(st, DW_AT_type, DW_FORM_ref4, sub_pos);
				} dwarf_sec_end(&st->abbrev);
			} dwarf_end(st);
			break;
		}

		case type_ref_block:
			/* skip */
			this_start = st->info.length;
			dwarf_type(st, ty->ref);
			break;

		case type_ref_func:
		{
			decl **i;
			const unsigned pos_ref = dwarf_type(st, ty->ref);
			/*pos_sibling = pos_ref + ??? */

			this_start = st->info.length;

			dwarf_start(st); {
				dwarf_abbrev_start(st, DW_TAG_subroutine_type, DW_CHILDREN_yes); {
					/*dwarf_attr(st, DW_AT_sibling, DW_FORM_ref4, pos_sibling);*/
					dwarf_attr(st, DW_AT_type, DW_FORM_ref4, pos_ref);
					dwarf_attr(st, DW_AT_prototyped, DW_FORM_flag, 1);
				} dwarf_sec_end(&st->abbrev);
			} dwarf_end(st);

			for(i = ty->bits.func.args->arglist;
			    i && *i;
			    i++)
			{
				const unsigned sub_pos = dwarf_type(st, (*i)->ref);

				dwarf_start(st); {
					dwarf_abbrev_start(st, DW_TAG_formal_parameter, DW_CHILDREN_no); {
						dwarf_attr(st, DW_AT_type, DW_FORM_ref4, sub_pos);
					} dwarf_sec_end(&st->abbrev);
				} dwarf_end(st);
			}
			break;
		}

		case type_ref_array:
		{
			int have_sz = !!ty->bits.array.size;
			intval_t sz;
			const unsigned sub_pos = dwarf_type(st, ty->ref);

			this_start = st->info.length;

			if(have_sz)
				sz = const_fold_val(ty->bits.array.size);

			dwarf_start(st); {
				dwarf_abbrev_start(st, DW_TAG_array_type, DW_CHILDREN_yes); {
					dwarf_attr(st, DW_AT_type, DW_FORM_ref4, sub_pos);
					/*dwarf_attr(st, DW_AT_sibling, DW_FORM_ref4, "???");*/
				} dwarf_sec_end(&st->abbrev);

				if(have_sz){
					dwarf_abbrev_start(st, DW_TAG_subrange_type, DW_CHILDREN_yes); {
						dwarf_attr(st, DW_AT_lower_bound, DW_FORM_data1, 0);
						dwarf_attr(st, DW_AT_upper_bound, DW_FORM_data1, sz);
					} dwarf_sec_end(&st->abbrev);
				}
			} dwarf_end(st);
			break;
		}

		case type_ref_cast:
		{
			const unsigned sub_pos = dwarf_type(st, ty->ref);
			this_start = st->info.length;

			if(ty->bits.cast.is_signed_cast){
				/* skip */
			}else{
				dwarf_start(st); {
					dwarf_abbrev_start(st, DW_TAG_const_type, DW_CHILDREN_yes); {
						dwarf_attr(st, DW_AT_type, DW_FORM_ref4, sub_pos);
					} dwarf_sec_end(&st->abbrev);
				} dwarf_end(st);
			}
			break;
		}
	}

	return this_start;
}

static void dwarf_cu(struct dwarf_state *st, const char *fname)
{
	dwarf_start(st); {
		dwarf_abbrev_start(st, DW_TAG_compile_unit, DW_CHILDREN_yes); {
			dwarf_attr(st, DW_AT_producer, DW_FORM_string, "ucc development version");
			dwarf_attr(st, DW_AT_language, DW_FORM_data2, DW_LANG_C99);
			dwarf_attr(st, DW_AT_name, DW_FORM_string, fname);
			dwarf_attr(st, DW_AT_low_pc, DW_FORM_addr, 0x12345); /* TODO */
			dwarf_attr(st, DW_AT_high_pc, DW_FORM_addr, 0x54321);
		} dwarf_sec_end(&st->abbrev);
	} dwarf_end(st);
}

static void dwarf_info_header(struct dwarf_sec *sec)
{
	/* hacky? */
	fprintf(sec->f,
			"\t.long .Ldbg_info_end - .Ldbg_info_start\n"
			".Ldbg_info_start:\n"
			"\t.short 2 # DWARF 2\n"
			"\t.long 0  # abbrev offset\n"
			"\t.byte %d  # sizeof(void *)\n",
			platform_word_size());

	sec->length += 4 + 2 + 4 + 1;
}

static void dwarf_info_footer(struct dwarf_sec *sec)
{
	fprintf(sec->f, ".Ldbg_info_end:\n");
}

static void dwarf_global_variable(struct dwarf_state *st, decl *d)
{
	unsigned typos;

	if(!d->spel)
		return;

	typos = dwarf_type(st, d->ref);

	dwarf_start(st); {
		dwarf_abbrev_start(st, DW_TAG_variable, DW_CHILDREN_no); {
			dwarf_attr(st, DW_AT_name, DW_FORM_string, d->spel);
			dwarf_attr(st, DW_AT_type, DW_FORM_ref4, typos);
			/*dwarf_attr(st, DW_AT_location, DW_FORM_block1, d->spel_asm);*/
		} dwarf_sec_end(&st->abbrev);
	} dwarf_end(st);
}

void out_dbginfo(symtable_global *globs, const char *fname)
{
	struct dwarf_state st = {
		DWARF_SEC_INIT(cc_out[SECTION_DBG_ABBREV]),
		DWARF_SEC_INIT(cc_out[SECTION_DBG_INFO]),
	};

	dwarf_info_header(&st.info);

	/* output abbrev Compile Unit header */
	dwarf_cu(&st, fname);

	/* output subprograms */
	{
		decl **diter;
		for(diter = globs->stab.decls; diter && *diter; diter++){
			decl *d = *diter;

			if(DECL_IS_FUNC(d)){
				; /* TODO: dwarf_subprogram_func(&st, d); */
			}else{
				/* TODO: dump type unless seen */
				dwarf_global_variable(&st, d);
			}
		}
	}

	dwarf_info_footer(&st.info);
}
