#define AR_LEN(x) sizeof x / sizeof *x

pa(int *a, int n)
{
	for(int i = 0; i < n; i++)
		printf("[%d] = %d\n", i, a[i]);
}

main()
{
	int n = 5;
	int a[n];

	for(int i = 0; i < n; i++)
		a[i] = i + 1;

	printf("sizeof a = %d\n", AR_LEN(a));

	pa(a, AR_LEN(a));
}
