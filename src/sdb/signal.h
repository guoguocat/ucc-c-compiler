#ifndef HANDLE_H
#define HANDLE_H

typedef enum
{
	HANDLE_PRINT      = 1 << 0,
	HANDLE_HEREDITARY = 1 << 1,
	HANDLE_STOP       = 1 << 2,
} handle_mask;

handle_mask sig_handle_mask(int sig);
void sig_handle_mask_set(int sig, handle_mask m);

int         sig_parse(const char *);
const char *sig_name(int);

#endif
