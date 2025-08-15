#ifndef _T86VM_H
#define _T86VM_H

#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>

typedef struct cpu8086_regs {
	int16_t ax;
	int16_t bx;
	int16_t cx;
	int16_t dx;
	int16_t pc;
	int16_t sp;
	int16_t bp;
	int16_t si;
	int16_t di;
	uint16_t es;
	uint16_t cs;
	uint16_t ss;
	uint16_t ds;
	uint16_t fs;
	uint16_t eflags;
} cpu8086_regs_t;

typedef struct cpu8086_dtr {
	uint16_t limit;
	uint32_t base;
} cpu8086_dtr_t;

typedef struct cpu8086 {
	cpu8086_regs_t regs;
	cpu8086_dtr_t gdtr;
	cpu8086_dtr_t idtr;
	int intr; //interrupt line
	int reset; //reset line
} cpu8086_t;

#define EFLAGS_CF (1 << 0)
#define EFLAGS_PF (1 << 2)
#define EFLAGS_AF (1 << 4)
#define EFLAGS_ZF (1 << 6)
#define EFLAGS_SF (1 << 6)
#define EFLAGS_IF (1 << 9)
#define EFLAGS_DF (1 << 10)
#define EFLAGS_OF (1 << 11)

typedef struct eprom {
	uint32_t base;
	uint32_t size;
	char *data;
} eprom_t;

typedef struct t86vm_ctx {
	FILE *floppy;
	cpu8086_t cpu;
	eprom_t *bios;
	size_t ram_size;
	char *ram;
	jmp_buf jmperr;
	int32_t seg_overide;
} t86vm_ctx_t;

int emul(t86vm_ctx_t *);
int cpu8086_emu(t86vm_ctx_t *);
void cpu8086_reset(cpu8086_t *);
eprom_t *eprom_open(const char *path);
uint32_t eprom_read(eprom_t *eprom,uint32_t addr,size_t size,int *error);
uint32_t mem_read(t86vm_ctx_t *ctx,uint32_t addr,size_t size,int *error);
void mem_write(t86vm_ctx_t *ctx,uint32_t addr,uint32_t data,size_t size,int *error);
void error(const char *fmt,...);

#endif
