/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
// 9 and 15 are reserved
TRAPHANDLER_NOEC(ENTRY_DIVIDE, T_DIVIDE)	// divide error
TRAPHANDLER_NOEC(ENTRY_DEBUG, T_DEBUG)		// debug exception
TRAPHANDLER_NOEC(ENTRY_NMI, T_NMI)		// non-maskable interrupt
TRAPHANDLER_NOEC(ENTRY_BRKPT, T_BRKPT)		// breakpoint
TRAPHANDLER_NOEC(ENTRY_OFLOW, T_OFLOW)		// overflow
TRAPHANDLER_NOEC(ENTRY_BOUND, T_BOUND)		// bounds check
TRAPHANDLER_NOEC(ENTRY_ILLOP, T_ILLOP)		// illegal opcode
TRAPHANDLER_NOEC(ENTRY_DEVICE, T_DEVICE)	// device not available
TRAPHANDLER(ENTRY_DBLFLT, T_DBLFLT)		// double fault
TRAPHANDLER(ENTRY_TSS, T_TSS)			// invalid task switch segment
TRAPHANDLER(ENTRY_SEGNP, T_SEGNP)		// segment not present
TRAPHANDLER(ENTRY_STACK, T_STACK)		// stack exception
TRAPHANDLER(ENTRY_GPFLT, T_GPFLT)		// general protection fault
TRAPHANDLER(ENTRY_PGFLT, T_PGFLT)		// page fault
TRAPHANDLER_NOEC(ENTRY_FPERR, T_FPERR)		// floating point error
TRAPHANDLER(ENTRY_ALIGN, T_ALIGN)		// alignment check
TRAPHANDLER_NOEC(ENTRY_MCHK, T_MCHK)		// machine check
TRAPHANDLER_NOEC(ENTRY_SIMDERR, T_SIMDERR)	// SIMD floating point error
TRAPHANDLER_NOEC(ENTRY_SYSCALL, T_SYSCALL)	// system call

TRAPHANDLER_NOEC( ENTRY_IRQ_TIMER, IRQ_OFFSET + IRQ_TIMER)	// 0 IRQ_TIMER
TRAPHANDLER_NOEC( ENTRY_IRQ_KBD, IRQ_OFFSET + IRQ_KBD)		// 1 IRQ_KBD
TRAPHANDLER_NOEC( ENTRY_IRQ_2, IRQ_OFFSET + 2)			// 2 IRQ_2
TRAPHANDLER_NOEC( ENTRY_IRQ_3, IRQ_OFFSET + 3)			// 3 IRQ_3
TRAPHANDLER_NOEC( ENTRY_IRQ_SERIAL, IRQ_OFFSET + IRQ_SERIAL)	// 4 IRQ_SERIAL
TRAPHANDLER_NOEC( ENTRY_IRQ_5, IRQ_OFFSET + 5)			// 5 IRQ_5
TRAPHANDLER_NOEC( ENTRY_IRQ_6, IRQ_OFFSET + 6)			// 6 IRQ_6
TRAPHANDLER_NOEC( ENTRY_IRQ_SPURIOUS, IRQ_OFFSET + IRQ_SPURIOUS)// 7 IRQ_SPURIOUS
TRAPHANDLER_NOEC( ENTRY_IRQ_8, IRQ_OFFSET + 8)			// 8 IRQ_8
TRAPHANDLER_NOEC( ENTRY_IRQ_9, IRQ_OFFSET + 9)			// 9 IRQ_9
TRAPHANDLER_NOEC( ENTRY_IRQ_10, IRQ_OFFSET + 10)		// 10 IRQ_10
TRAPHANDLER_NOEC( ENTRY_IRQ_11, IRQ_OFFSET + 11)		// 11 IRQ_11
TRAPHANDLER_NOEC( ENTRY_IRQ_12, IRQ_OFFSET + 12)		// 12 IRQ_12
TRAPHANDLER_NOEC( ENTRY_IRQ_13, IRQ_OFFSET + 13)		// 13 IRQ_13
TRAPHANDLER_NOEC( ENTRY_IRQ_IDE, IRQ_OFFSET + IRQ_IDE)		// 14 IRQ_IDE
TRAPHANDLER_NOEC( ENTRY_IRQ_15, IRQ_OFFSET + 15)		// 15 IRQ_15
TRAPHANDLER_NOEC( ENTRY_IRQ_ERROR, IRQ_OFFSET + IRQ_ERROR)	// 19 IRQ_ERROR

/*
 * Lab 3: Your code here for _alltraps
 */
.globl _alltraps;
.type _alltraps,@function;
.align 2;
_alltraps:
	pushl %ds
	pushl %es
	pushal
	movl $GD_KD, %eax
	movw %ax, %ds
	movw %ax, %es
	pushl %esp
	call trap

.global sysenter_handler
.type sysenter_handler, @function					
.align 2
sysenter_handler:
	pushl %esi
	pushl %edi 
	pushl %ebx
	pushl %ecx 
	pushl %edx
	pushl %eax
	call syscall
	movl %esi, %edx
	movl %ebp, %ecx
	sysexit
