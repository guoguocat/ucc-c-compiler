main()
{
	void *jmps[3];

	int i = 0;

	jmps[0] = &&a, jmps[1] = &&b, jmps[2] = &&c;

	goto *jmps[i];

a:
	i = 1;
	goto fin;

b:
	i = 2;
	goto fin;

c:
	i = 3;
	goto fin;

fin:
	return i;
}
