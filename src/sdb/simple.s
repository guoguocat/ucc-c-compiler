.globl _start
_start:
	#movl $35, %eax # SYS_nanosleep

	#pushq $0 # nano
	#pushq $60 # sec

	#movq %rsp, %rdi
	#xorq %rsi, %rsi
	#syscall

	movl $0, %ebx

for:
	call print
	incl %ebx
	cmp $10, %ebx
	jne for

	movl $5, %edi
	call exit

exit:
	movl $60, %eax
	syscall
	hlt

print:
	pushq %rbx
	subq $8, %rsp
	movl $0x0a6968, (%rsp)
	movl $1, %edi
	movq %rsp, %rsi
	movl $3, %edx
	movl $1, %eax
	syscall
	addq $8, %rsp
	popq %rbx
	ret
