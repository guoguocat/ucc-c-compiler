#include <wchar.h>

main()
{
	wchar_t c = L'a';
	wchar_t s[] = {
		'a', // extended
		L'b'
	};

	return c + s[0] - s[1] + L'\1'; // 97
}