// TEST: %ucc %s -o %t

main()
{
	struct {
		int x;
		struct {
			int y, z;
			struct {
				int u, v;
			};
		};
	} a;
	return a.x + a.y + a.z + a.u + a.v;
}
