#define ASSERT(md, sz)                                    \
_Static_assert(sizeof(int __attribute((mode(md)))) == sz, \
	#md " should be " #sz " on x86")

ASSERT(QI, 1);
ASSERT(HI, 2);
ASSERT(SI, 4);
ASSERT(DI, 8);
