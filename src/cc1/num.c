#include "num.h"

numeric *numeric_new(long v)
{
	numeric *num = umalloc(sizeof *num);
	num->val.i = v;
	return num;
}

int numeric_cmp(const numeric *a, const numeric *b)
{
	UCC_ASSERT(
			(a->suffix & VAL_FLOATING) == (b->suffix & VAL_FLOATING),
			"cmp int and float?");

	if(a->suffix & VAL_FLOATING){
		const floating_t fa = a->val.f, fb = b->val.f;

		if(fa > fb)
			return 1;
		if(fa < fb)
			return -1;
		return 0;

	}else{
		const integral_t la = a->val.i, lb = b->val.i;

		if(la > lb)
			return 1;
		if(la < lb)
			return -1;
		return 0;
	}
}

int integral_str(char *buf, size_t nbuf, integral_t v, type *ty)
{
	/* the final resting place for an integral */
	const int is_signed = type_is_signed(ty);

	if(ty){
		sintegral_t sv;
		v = integral_truncate(v, type_size(ty, NULL), &sv);
		if(is_signed)
			v = sv;
	}

	return snprintf(
			buf, nbuf,
			is_signed
				? "%" NUMERIC_FMT_D
				: "%" NUMERIC_FMT_U,
			v, is_signed);
}

integral_t integral_truncate_bits(
		integral_t val, unsigned bits,
		sintegral_t *signed_iv)
{
	integral_t pos_mask = ~(~0ULL << bits);
	integral_t truncated = val & pos_mask;

	if(fopt_mode & FOPT_CAST_W_BUILTIN_TYPES){
		/* we use sizeof our types so our conversions match the target conversions */
#define BUILTIN(type)                    \
		if(bits == sizeof(type) * CHAR_BIT){ \
			if(signed_iv)                      \
				*signed_iv = (signed type)val;   \
			return (unsigned type)val;         \
		}

		BUILTIN(char);
		BUILTIN(short);
		BUILTIN(int);
		BUILTIN(long);
		BUILTIN(long long);
	}

	/* not builtin - bitfield, etc */
	if(signed_iv){
		const unsigned shamt = CHAR_BIT * sizeof(val) - bits;

		/* implementation specific signed right shift.
		 * this is to sign extend the value
		 */
		*signed_iv = (sintegral_t)val << shamt >> shamt;
	}

	return truncated;
}

integral_t integral_truncate(
		integral_t val, unsigned bytes,
		sintegral_t *sign_extended)
{
	return integral_truncate_bits(
			val, bytes * CHAR_BIT,
			sign_extended);
}

int integral_high_bit(integral_t val, type *ty)
{
	if(type_is_signed(ty)){
		const sintegral_t as_signed = val;

		if(as_signed < 0)
			val = integral_truncate(val, type_size(ty, &ty->where), NULL);
	}

	{
		int r;
		for(r = -1; val; r++, val >>= 1);
		return r;
	}
}
