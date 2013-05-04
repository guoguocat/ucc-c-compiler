#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>

#include <sys/user.h>
#include <sys/ptrace.h>

#include <mach/mach.h>
#include <mach/mach_vm.h>

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

static int darwin_set_errno(kern_return_t kr)
{
	switch(kr){
		case KERN_SUCCESS:
			errno = 0;
			return 0;

		case KERN_PROTECTION_FAILURE:
		case KERN_INVALID_ADDRESS:
			errno = EFAULT;
			break;
		case KERN_NO_SPACE:
			errno = ENOMEM;
			break;
		case KERN_INVALID_ARGUMENT:
		default:
			errno = EINVAL;
	}
	return -1;
}

int arch_mem_read(const struct arch_proc *ap, addr_t addr, word_t *p)
{
	struct arch_proc_darwin *dap = (struct arch_proc_darwin *)ap;

	unsigned long n = 1; /* one word please */

	/* why vm_read* takes a long instead of a pointer I'll never know.. */
	kern_return_t kr = vm_read_overwrite(
			dap->port, addr, sizeof *p, (word_t)p, &n);

	return darwin_set_errno(kr);
}

int arch_mem_write(const struct arch_proc *ap, addr_t addr, word_t l)
{
	/* typedefs... typedefs everywhere...
	 * still less portable than using longs.
	 * vm_* for 32-bit and mach_vm_* for 64. ILP64?
	 */
	struct arch_proc_darwin *dap = (struct arch_proc_darwin *)ap;

	/* read a page... to change a single word... */
	vm_size_t page_sz = 0x1000;
	host_page_size(mach_host_self(), &page_sz);

	mach_vm_address_t addr_page = addr & ~(page_sz - 1);
	mach_vm_address_t off = addr - addr_page;
	mach_msg_type_number_t whole_page_sz = page_sz;
	pointer_t whole_page;

	/* allocate a page... */
	kern_return_t kr = mach_vm_read(
			dap->port, addr_page, page_sz,
			&whole_page, &whole_page_sz);

	if(kr != KERN_SUCCESS)
		goto bad;

	/* change the single word... */
	*(word_t *)((char *)whole_page + off) = l;

	/* just let us */
	mach_vm_protect(
			dap->port, addr_page, page_sz,
			0, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);

	/* write the whole page back... */
	kr = mach_vm_write(dap->port, addr_page, whole_page, whole_page_sz);

	/* and deallocate our copy of the page */
	mach_vm_deallocate(mach_task_self(), whole_page, whole_page_sz);

bad:
	return darwin_set_errno(kr);
}

int arch_reg_read(const struct arch_proc *ap, int off, reg_t *p)
{
	/* TODO: task/thread get state */
	errno = ENOSYS;
	return -1;
}

int arch_reg_write(const struct arch_proc *ap, int off, const reg_t v)
{
	/* TODO: task/thread set state */
	errno = ENOSYS;
	return -1;
}
