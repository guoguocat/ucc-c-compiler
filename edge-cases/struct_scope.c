struct A
{
	int i;
};

main()
{
	//struct A; // scopes a new A
	struct B { struct A *p; int b; };// mistakenly refers to outside B
	struct A { struct B *p; int a; };
}
