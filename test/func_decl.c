typedef int size_t;

old(argc, argv)
    int argc;
    char **argv;
{
}

old_with_tdef(x)
	size_t x;
{
}

new_with_typedef(size_t);
new_with_typedef(size_t a);
new_with_typedef(size_t a)
{
}

normal(int a)
{
}
