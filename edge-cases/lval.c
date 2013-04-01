main()
{
	int x;
	struct A { int a; } s1, s2, sf();
	int i = f();

	sf(3).a = 1;
	(s1=s2).a = 1;
	((i==6)?s1:s2).a = 1;
	(x,s1).a = 1;
}
