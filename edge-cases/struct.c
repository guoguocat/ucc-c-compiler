main()
{
	struct A
	{
		int i, j;
	} s1, s2, s3;

	int x;

	s1 = (s2 = s3);
	//sf(x);
	(x, s1);
	x ? s1 : s2;
}
