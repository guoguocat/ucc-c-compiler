struct A
{
	int i, j;
	int k;
};

init(struct A *p)
{
	p->j = 4;
}

main()
{
	struct A b;

	init(&b);

	return b.j;
}
