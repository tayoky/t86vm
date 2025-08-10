#ifndef _T86VM_H
#define _T86VM_H

#include <stdint.h>
#include <stdio.h>

typedef struct regs {
	uint16_t ax;
	uint16_t bx;
	uint16_t cx;
	uint16_t dx;
	uint16_t pc;
	uint16_t sp;
	uint16_t bp;
	uint16_t si;
	uint16_t di;
	uint16_t es;
	uint16_t cs;
	uint16_t ss;
	uint16_t ds;
	uint16_t fs;
	uint16_t eflags;
} regs_t;

#define EFLAGS_IF (1 << 9)
#define EFLAGS_DF (1 << 10)

typedef struct t86vm_ctx {
	FILE *floppy;
	regs_t regs;
	size_t ram_size;
	char *ram;
} t86vm_ctx_t;

int emul(t86vm_ctx_t *);
int emu86(t86vm_ctx_t *);
void error(const char *fmt,...);

#endif
