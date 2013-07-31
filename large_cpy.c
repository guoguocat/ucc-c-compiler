#define SZ 32

typedef __SIZE_TYPE__ size_t;

blk_cpy(char *d, char *s, size_t n)
{
	struct block
	{
		char data[SZ];
	} *d_ = (void *)d, *s_ = (void *)s;

	for(; n > 0; n--)
		*d_++ = *s_++;
}

main()
{
	char data[256];

	for(int i = 0; i < 128; i++)
		data[i] = i;

	blk_cpy(&data[128], &data[0], 4);

	int t = 0;
	for(int i = 0; i < 256; i++)
		t += data[i];

	printf("%u\n", t);
}
