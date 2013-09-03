#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../util/util.h"
#include "../util/where.h"

#include "data_structs.h"
#include "cc1.h"
#include "fold.h"
#include "sue.h"
#include "const.h"

#include "pack.h"
#include "defs.h"

#include "fold_sue.h"

static void struct_pack(
		decl *d, unsigned *poffset, unsigned sz, unsigned align)
{
	unsigned after_space;

	pack_next(poffset, &after_space, sz, align);
	/* offset is the end of the decl, after_space is the start */

	d->struct_offset = after_space;
}

static void struct_pack_finish_bitfield(
		unsigned *poffset, unsigned *pbitfield_current)
{
	/* gone from a bitfield to a normal field - pad by the overflow */
	unsigned change = *pbitfield_current / CHAR_BIT;

	*poffset = pack_to_align(*poffset + change, 1);

	*pbitfield_current = 0;
}

static void bitfield_size_align(
		type_ref *tref, unsigned *psz, unsigned *palign, where *from)
{
	/* implementation defined if ty isn't one of:
	 * unsigned, signed or _Bool.
	 * We make it take that align,
	 * and reserve a max. of that size for the bitfield
	 */
	const type *ty;
	tref = type_ref_is_type(tref, type_unknown);
	assert(tref);

	ty = tref->bits.type;

	*psz = type_size(ty, from);
	*palign = type_align(ty, from);
}

static void fold_enum(struct_union_enum_st *en, symtable *stab)
{
	const int has_bitmask = !!attr_present(en->attr, attr_enum_bitmask);
	sue_member **i;
	int defval = has_bitmask;

	for(i = en->members; i && *i; i++){
		enum_member *m = (*i)->enum_member;
		expr *e = m->val;

		/* -1 because we can't do dynarray_add(..., 0) */
		if(e == (expr *)-1){

			EOF_WHERE(&en->where,
				m->val = expr_new_val(defval)
			);

			if(has_bitmask)
				defval <<= 1;
			else
				defval++;

		}else{
			integral_t v;

			FOLD_EXPR(e, stab);

			fold_check_expr(e,
					FOLD_CHK_INTEGRAL | FOLD_CHK_CONST_I,
					"enum constant");

			v = const_fold_val_i(e);
			m->val = e;

			defval = has_bitmask ? v << 1 : v + 1;
		}
	}
}

void fold_sue(struct_union_enum_st *const sue, symtable *stab)
{
	if(sue->folded || !sue->complete)
		return;
	sue->folded = 1;

	if(sue->primitive == type_enum){
		fold_enum(sue, stab);

	}else{
		unsigned bf_cur_lim;
		unsigned align_max = 1;
		unsigned sz_max = 0;
		unsigned offset = 0;
		int realign_next = 0;
		struct
		{
			unsigned current_off, first_off;
		} bitfield;
		sue_member **i;

		memset(&bitfield, 0, sizeof bitfield);

		if(attr_present(sue->attr, attr_packed))
			ICE("TODO: __attribute__((packed)) support");

		for(i = sue->members; i && *i; i++){
			decl *d = (*i)->struct_member;
			unsigned align, sz;
			struct_union_enum_st *sub_sue;

			fold_decl(d, stab, NULL);

			if((sub_sue = type_ref_is_s_or_u_or_e(d->ref))){
				if(sub_sue != sue)
					fold_sue(sub_sue, stab);

				if(type_ref_is(d->ref, type_ref_ptr) || sub_sue->primitive == type_enum)
					goto normal;

				if(sub_sue == sue)
					die_at(&d->where, "nested %s", sue_str(sue));
				else if(sub_sue->flexarr && i[1])
					warn_at(&d->where, "embedded struct with flex-array not final member");

				sz = sue_size(sub_sue, &d->where);
				align = sub_sue->align;

			}else if(d->field_width){
				const unsigned bits = const_fold_val_i(d->field_width);

				sz = align = 0; /* don't affect sz_max or align_max */

				if(bits == 0){
					/* align next field / treat as new bitfield
					 * note we don't pad here - we don't want to
					 * take up any space with this field
					 */
					realign_next = 1;

					/* also set struct_offset for 0-len bf, for pad reasons */
					d->struct_offset = offset;

				}else if(realign_next
				|| !bitfield.current_off
				|| bitfield.current_off + bits > bf_cur_lim)
				{
					if(realign_next || bitfield.current_off){
						if(!realign_next){
							/* bitfield overflow - repad */
							warn_at(&d->where, "bitfield overflow (%d + %d > %d) - "
									"moved to next boundary", bitfield.current_off, bits,
									bf_cur_lim);
						}else{
							realign_next = 0;
						}

						/* don't pay attention to the current bitfield offset */
						bitfield.current_off = 0;
						struct_pack_finish_bitfield(&offset, &bitfield.current_off);
					}

					bf_cur_lim = CHAR_BIT * type_ref_size(d->ref, &d->where);

					/* Get some initial padding.
					 * Note that we want to affect the align_max
					 * of the struct and the size of this field
					 */
					bitfield_size_align(d->ref, &sz, &align, &d->where);

					/* we are onto the beginning of a new group */
					struct_pack(d, &offset, sz, align);
					bitfield.first_off = d->struct_offset;
					d->first_bitfield = 1;

				}else{
					/* mirror previous bitfields' offset in the struct
					 * difference is in .struct_offset_bitfield
					 */
					d->struct_offset = bitfield.first_off;
				}

				d->struct_offset_bitfield = bitfield.current_off;
				bitfield.current_off += bits; /* allowed to go above sizeof(int) */

				if(bitfield.current_off == bf_cur_lim){
					/* exactly reached the limit, reset bitfield indexing */
					bitfield.current_off = 0;
				}

			}else{
normal:
				align = decl_align(d);
				if(type_ref_is_incomplete_array(d->ref)){
					if(i[1])
						die_at(&d->where, "flexible array not at end of struct");
					else if(sue->primitive != type_struct)
						die_at(&d->where, "flexible array in a %s", sue_str(sue));
					else if(i == sue->members) /* nothing currently */
						warn_at(&d->where, "struct with just a flex-array is an extension");

					sue->flexarr = 1;
					sz = 0; /* not counted in struct size */
				}else{
					sz = decl_size(d);
				}
			}

			if(sue->primitive == type_struct && !d->field_width){
				const int prev_offset = offset;

				if(bitfield.current_off){
					/* we automatically pad on the next struct_pack,
					 * don't struct_pack() here */
					bitfield.current_off = 0;
				}

				struct_pack(d, &offset, sz, align);

				{
					int pad = d->struct_offset - prev_offset;
					if(pad){
						cc1_warn_at(&d->where, 0, WARN_PAD,
								"padding '%s' with %d bytes to align '%s'",
								sue->spel, pad, decl_to_str(d));
					}
				}
			}

			if(align > align_max)
				align_max = align;
			if(sz > sz_max)
				sz_max = sz;
		}

		sue->align = align_max;
		sue->size = pack_to_align(
				sue->primitive == type_struct ? offset : sz_max,
				align_max);
	}
}
