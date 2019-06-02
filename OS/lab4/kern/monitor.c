// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>
#include <kern/trap.h>
#include <kern/pmap.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display current stack backtrace", mon_backtrace },
	{ "showmappings", "Display all physical page mappings in a given virtual address range", mon_showmappings },
	{ "setperm", "Set the permissions of mapping in given virtual address", mon_setperm },
	{ "dump", "Dump the contents of a given range of memory", mon_dump }
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
    char *pret_addr;

	// Your code here.
	pret_addr = (char*)read_pretaddr();
	*(uint32_t*)(pret_addr+4) = *(uint32_t*)pret_addr;	// save return address to 4 bytes above the original place to return normally later
	*(uint32_t*)pret_addr = (uint32_t)do_overflow;		// inject target code to original return address
}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	// Your code here.
	cprintf("Stack backtrace:\n");
	uint32_t ebp = read_ebp();		// value of current ebp
	// backtrace terminates when ebp = 0
	while(ebp != 0)
	{
		uint32_t eip = *(uint32_t*)(ebp+4);		// return address
		cprintf("  eip %08x  ebp %08x  args %08x %08x %08x %08x %08x\n", eip, ebp, *(uint32_t*)(ebp+8), *(uint32_t*)(ebp+12), *(uint32_t*)(ebp+16), *(uint32_t*)(ebp+20), *(uint32_t*)(ebp+24));
		struct Eipdebuginfo info;
		if (debuginfo_eip(eip, &info) >= 0)
		{
			// use eip_fn_namelen to restrict the length of function name
			cprintf("         %s:%d %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, eip - info.eip_fn_addr);
		}
		ebp = *(uint32_t*)ebp;
	}
	overflow_me();
	cprintf("Backtrace success\n");
	return 0;
}

/*
 * challenges for lab 2
 */
int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3)
	{
		cprintf("usage: showmappings [begin_va] [end_va]\n");
		return 0;
	}

	uintptr_t begin = ROUNDDOWN(strtol(argv[1], NULL, 16), PGSIZE);
	uintptr_t end = ROUNDDOWN(strtol(argv[2], NULL, 16), PGSIZE);
	for (uintptr_t i = begin; i <= end; i += PGSIZE)
	{
		pte_t *pte = pgdir_walk(kern_pgdir, (void *)i, 0);
		if (pte)
		{
			cprintf("0x%x -> 0x%x %x%x%x\n", i, PADDR((void *)i), (*pte & PTE_P)>0, (*pte & PTE_W)>0, (*pte & PTE_U)>0);
		}
		else
		{
			cprintf("0x%x -> 0x%x 000\n");
		}
	}
	return 0;
}

int
mon_setperm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3)
	{
		cprintf("usage: setperm [va] [perm_bits('u/s'+'r/w')]\n");
		return 0;
	}

	uintptr_t addr = strtol(argv[1], NULL, 16);
	char *perm = argv[2];

	pte_t *pte = pgdir_walk(kern_pgdir, (void *)addr, 0);
	if (!pte)
	{
		cprintf("invalid virtual address\n");
		return 0;
	}

	if (perm[0] == 'u')
	{
		*pte |= PTE_U;
	}
	else if (perm[0] == 's')
	{
		*pte &= ~PTE_U;
	}
	if (perm[1] == 'w')
	{
		*pte |= PTE_W;
	}
	else if (perm[1] == 'r')
	{
		*pte &= ~PTE_W;
	}
	return 0;
}

int
mon_dump(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 4)
	{
		cprintf("usage: dump [virtual/physical('v'/'p')] [address] [range]\n");
		return 0;
	}

	char *addr_type = argv[1];
	uintptr_t addr = strtol(argv[2], NULL, 16);
	if (!strcmp(addr_type,"p"))
	{
		addr = (uintptr_t)KADDR(addr);
	}
	uint32_t range = strtol(argv[3], NULL, 10);

	addr = ROUNDDOWN(addr, sizeof(uintptr_t));
	uintptr_t end = ROUNDDOWN(addr+range, sizeof(uintptr_t));
	for (uintptr_t i = addr; i <= end; i += sizeof(uintptr_t))
	{
		pte_t *pte = pgdir_walk(kern_pgdir, (void *)i, 0);
		if (!pte || !(*pte & PTE_P))
		{
			cprintf("invalid address %x\n", i);
			return 0;
		}
		cprintf("0x%x: %x\n", i, *(uint32_t*)i);
	}
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");

	if (tf != NULL)
		print_trapframe(tf);

	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
