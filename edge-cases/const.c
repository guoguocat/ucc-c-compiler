main()
{
	const struct A { int a, b; } x;
	struct A y;

	x.a = 1;
	y.a = 1;

	typedef const struct B { int a, b; } B_t;
	B_t a;
	B_t b;

	a.a = 1;
	b.a = 1;
}
