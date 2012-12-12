// RUN: %ucc -c %s
// RUN: %ucc %s -S -o- | grep 'call.*f'; [ $? -ne 0 ]
// RUN: [ `%ucc %s -S -o- | grep 'call.*[ghi]' | wc -l` -eq 3 ]

test_while()
{
	while(0)
		f();
}

test_if()
{
	if(0)
		f();

	goto x;

	if(0){
x:
		print("hi");
	}

	if(1)
		g();

	if(h())
		i();
}
