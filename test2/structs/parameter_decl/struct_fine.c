struct A;

f(struct A *);

test1()
{
	struct A
	{
		int i, j;
	} a;

	f(&a); // CHECK: /warning: mismatching argument/
}

struct A
{
	int i, j;
};

test2()
{
	struct A a;
	f(&a); // CHECK: !/warning/
}
