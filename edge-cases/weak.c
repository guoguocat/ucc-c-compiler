extern void f(void) __attribute((weak));

main()
{
	printf("%p\n", f);
}
