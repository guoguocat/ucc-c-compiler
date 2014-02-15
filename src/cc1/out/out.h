#ifndef OUT_H
#define OUT_H

#define OUT_VPHI_SZ 64 /* sizeof(struct vstack) */

void out_pop(void);
void out_pop_func_ret(type_ref *func_ty) ucc_nonnull((1));

void out_phi_pop_to(void *); /* put the current value into a phi-save area */
void out_phi_join(void *);   /* join vtop and the phi-save area */

void out_push_num(type_ref *t, const numeric *n) ucc_nonnull((1));
void out_push_l(type_ref *, long) ucc_nonnull((1));
void out_push_zero(type_ref *) ucc_nonnull((1));
void out_push_lbl(const char *s, int pic);
void out_push_noop(void);

void out_dup(void); /* duplicate top of stack */
void out_pulltop(int i); /* pull the nth stack entry to the top */
void out_normalise(void); /* change to 0 or 1 */

void out_push_sym(sym *);
void out_push_sym_val(sym *);
void out_store(void); /* store stack[1] into *stack[0] */

void out_set_bitfield(unsigned off, unsigned nbits);

void out_op(      enum op_type); /* binary ops and comparisons */
void out_op_unary(enum op_type); /* unary ops */
void out_deref(void);
void out_swap(void);
void out_flush_volatile(void);

void out_memcpy(unsigned long bytes); /* copies *vtop[0] -> *vtop[1] */

void out_cast(type_ref *to, int normalise_bool) ucc_nonnull((1));
void out_change_type(type_ref *) ucc_nonnull((1));
void out_set_lvalue(void);

void out_call(int nargs, type_ref *rt, type_ref *f) ucc_nonnull((2, 3));

void out_jmp(void); /* jmp to *pop() */
void out_jtrue( const char *);
void out_jfalse(const char *);

void out_func_prologue(
		type_ref *rf,
		int stack_res, int nargs, int variadic,
		int arg_offsets[]);

void out_func_epilogue(type_ref *);
void out_label(const char *);

void out_comment(const char *, ...) ucc_printflike(1, 2);
#ifdef ASM_H
void out_comment_sec(enum section_type, const char *, ...) ucc_printflike(2, 3);
#endif

void out_assert_vtop_null(void);
void out_dump(void);
void out_undefined(void);
void out_push_overflow(void);

void out_push_frame_ptr(int nframes);
void out_push_reg_save_ptr(void);
void out_push_nan(type_ref *ty);

int out_vcount(void);

#endif
