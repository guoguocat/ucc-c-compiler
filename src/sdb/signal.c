#include <stdio.h>
#include <string.h>
#include <signal.h>

#include "signal.h"

#define NSIGS 31
#define DEFAULT_MASK HANDLE_PRINT | HANDLE_STOP

static struct
{
	const char *nam;
	handle_mask msk;
} sigs[] = {
	[0 ... NSIGS-1] = { "?", DEFAULT_MASK },

#define SIG(x) [SIG##x].nam = "SIG"#x
	SIG(HUP),  SIG(INT),  SIG(QUIT), SIG(ILL),
	SIG(TRAP), SIG(ABRT), SIG(FPE),  SIG(KILL),
	SIG(USR1), SIG(SEGV), SIG(USR2), SIG(PIPE),
	SIG(ALRM), SIG(TERM), SIG(CHLD), SIG(CONT),
	SIG(STOP), SIG(TSTP), SIG(TTIN), SIG(TTOU),
#undef SIG
};
static const int N_SIGS = sizeof(sigs) / sizeof(*sigs);


handle_mask sig_handle_mask(int sig)
{
	if(sig < 0 || sig >= NSIGS)
		return DEFAULT_MASK;

	return sigs[sig].msk;
}

void sig_handle_mask_set(int sig, handle_mask m)
{
	if(0 <= sig && sig < NSIGS)
		sigs[sig].msk = m;
}

const char *
sig_name(int sig)
{
	if(sig < N_SIGS && sigs[sig].nam)
		return sigs[sig].nam;

	static char buf[8];
	snprintf(buf, sizeof buf, "%d", sig);
	return buf;
}

int sig_parse(const char *s)
{
	int off = !strncmp(s, "SIG", 3) ? 0 : 3;

	for(int i = 0; i < N_SIGS; i++)
		if(sigs[i].nam && !strcmp(s, sigs[i].nam + off))
			return i;

	return -1;
}
