// RUN: %check -e %s

main()
{
	switch(1){
		case 2.3f: // CHECK: /error: case value not integral/
			;
	}
}
