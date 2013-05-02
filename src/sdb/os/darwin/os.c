#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>

#include <sys/user.h>
#include <sys/ptrace.h>

#include <mach/mach.h>

#include "../../arch.h"
#include "../../util.h" /* warn() */

static __attribute__((section ("__TEXT,__info_plist"), used))
const unsigned char task_for_pid_plist[] =
"<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
"<plist version=\"1.0\">\n"
" <dict>\n"
"  <key>CFBundleDevelopmentRegion</key>\n"
"  <string>English</string>\n"
"  <key>CFBundleIdentifier</key>\n"
"  <string>sdb</string>\n"
"  <key>CFBundleInfoDictionaryVersion</key>\n"
"  <string>6.0</string>\n"
"  <key>CFBundleName</key>\n"
"  <string>sdb</string>\n"
"  <key>CFBundleVersion</key>\n"
"  <string>1.0</string>\n"
"  <key>SecTaskAccess</key>\n"
"  <array>\n"
"   <string>allowed</string>\n"
"   <string>debug</string>\n"
"  </array>\n"
" </dict>\n"
"</plist>\n"
;

struct arch_proc_darwin
{
	struct arch_proc;
	task_t port;
};

int arch_reg_offset(const char *s)
{
	/* TODO */
	return -1;
}

int arch_pseudo_reg(enum pseudo_reg r)
{
	/* TODO */
	return -1;
}

struct arch_proc *arch_attach(pid_t pid)
{
	task_t port;
	kern_return_t err = task_for_pid(mach_task_self(), pid, &port);

	if(err){
		warn("task_for_pid(%d, pid=%d, [port]) failed: %d - may not be signed",
				mach_task_self(), pid, err);
		return NULL;
	}

	struct arch_proc_darwin *dap = calloc(1, sizeof *dap);

	assert(dap);

	dap->pid = pid;
	dap->port = port;

	return (struct arch_proc *)dap;
}

void arch_detach(struct arch_proc **pap)
{
	free(*pap);
	*pap = NULL;
}

int arch_mem_read(pid_t pid, addr_t addr, word_t *p)
{
	/* TODO: vm_read() */
	errno = ENOSYS;
	return -1;
}

int arch_mem_write(pid_t pid, addr_t addr, word_t l)
{
	/* TODO: vm_write() */
	errno = ENOSYS;
	return -1;
}

int arch_reg_read(pid_t pid, int off, reg_t *p)
{
	/* TODO: task/thread get state */
	errno = ENOSYS;
	return -1;
}

int arch_reg_write(pid_t pid, int off, const reg_t v)
{
	/* TODO: task/thread set state */
	errno = ENOSYS;
	return -1;
}
