main()
{
	static int i;

	return __builtin_constant_p(
					__builtin_constant_p(
						__builtin_types_compatible_p(int, typeof(i))));

	__builtin_trap();
}
