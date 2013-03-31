.globl _start
_start:
	movl $60, %eax
	movl $5, %edi
	syscall
	hlt
