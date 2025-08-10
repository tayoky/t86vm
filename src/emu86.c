#include "t86vm.h"

//an simple 8086 emulator wrote by tayoky

#define comp_addr(seg,reg) ((seg << 4) | reg)

uint8_t read_u8(t86vm_ctx_t *ctx){
	uint32_t addr = comp_addr(ctx->regs.cs,ctx->regs.pc++);
	if(addr >= ctx->ram_size){
		//OOB error
		//TODO : trigger an int
		error("OOB");
	}
	return ctx->ram[addr];
}

uint16_t read_u16(t86vm_ctx_t *ctx){
	return read_u8(ctx) | read_u8(ctx) << 8;
}

int push_u8(t86vm_ctx_t *ctx,uint8_t data){
	uint32_t addr = comp_addr(ctx->regs.ss,--ctx->regs.sp);
	if(addr >= ctx->ram_size){
		//OOB error
		//TODO : trigger an int
		error("OOB");
		return -1;
	}
	ctx->ram[addr] = data;
	return 0;
	
}
int push_u16(t86vm_ctx_t *ctx,uint16_t data){
	if(push_u8(ctx,data >> 8)<0)return -1;
	return push_u8(ctx,data);
}

uint16_t *reg(t86vm_ctx_t *ctx,uint8_t reg){
	switch(reg){
	case 0:
		return &ctx->regs.ax;
	case 1:
		return &ctx->regs.cx;
	case 2:
		return &ctx->regs.dx;
	case 3:
		return &ctx->regs.bx;
	case 4:
		return &ctx->regs.sp;
	case 5:
		return &ctx->regs.bp;
	case 6:
		return &ctx->regs.si;
	case 7:
		return &ctx->regs.di;
	default:
		error("unknow reg %d",reg);
		return NULL;
	}
}

uint16_t *seg(t86vm_ctx_t *ctx,uint8_t seg){
	switch(seg){
	case 0:
		return &ctx->regs.es;
	case 1:
		return &ctx->regs.cs;
	case 2:
		return &ctx->regs.ss;
	case 3:
		return &ctx->regs.ds;
	case 4:
		return &ctx->regs.es;
	default:
		error("unknow seg %d",reg);
		return NULL;
	}
}

int modrm(t86vm_ctx_t *ctx,int32_t *arg1,int32_t *arg2){
	uint8_t mod = read_u8(ctx);
	if((mod & 0xc0) != 0xc0){
		error("invalid opcode");
		return -1;
	}
	*arg2 = mod & 0x7;
	*arg1 = (mod >> 3) & 0x7;
	return 0;
}

//return on first interrupt
int emu86i(t86vm_ctx_t *ctx){

	uint8_t op = read_u8(ctx);
	int32_t arg1;
	int32_t arg2;
	switch(op){
	case 0x31:
	case 0x33: //xor
		//TODO : diff between 31 and 33 ????
		modrm(ctx,&arg1,&arg2);
		*reg(ctx,arg1) ^= *reg(ctx,arg2);
		break;
	case 0x40:
	case 0x41:
	case 0x42:
	case 0x43:
	case 0x44:
	case 0x45:
	case 0x46:
	case 0x47: //inc v?
		//TODO : flags
		printf("inc %x to %x\n",op - 0x40,++*reg(ctx,op - 0x40));
		break;
	case 0x48:
	case 0x49:
	case 0x4a:
	case 0x4b:
	case 0x4c:
	case 0x4d:
	case 0x4e:
	case 0x4f: //dec v?
		//TODO : flags
		printf("dec %x to %x\n",op - 0x48,--*reg(ctx,op - 0x48));
		break;
	case 0x8e: //mov (reg to seg)
		modrm(ctx,&arg1,&arg2);
		*seg(ctx,arg1) = *reg(ctx,arg2);
		break;
	case 0x90: //nop
		break;
	case 0xac: //lodsb
	case 0xad: //lodsw
		arg1 = comp_addr(ctx->regs.ds,ctx->regs.si);
		if((size_t)arg1 >= ctx->ram_size){
			error("OOB");
			return -1;
		}
		size_t size;
		if(op == 0xac){
			size = 1;
			ctx->regs.ax = (ctx->regs.ax & ~0xff) | ((uint8_t)ctx->ram[arg1]);
		} else {
			size = 2;
			ctx->regs.ax = (uint8_t)ctx->ram[arg1] | ((uint8_t)ctx->ram[arg1+1] << 8);
		}

		if(ctx->regs.eflags & EFLAGS_DF){
			ctx->regs.di -= size;
		} else {
			ctx->regs.di += size;
		}

		break;
	case 0xb0:
	case 0xb1:
	case 0xb2:
	case 0xb3:
	case 0xb4:
	case 0xb5:
	case 0xb6:
	case 0xb7: //mov (imm to reg) b
		arg2 = *reg(ctx,(op - 0xb0)%4);
		if(op >= 0xb4){
			//higger half
			arg2 |= read_u8(ctx) << 8;
		} else {
			//lower half
			arg2 |= read_u8(ctx);
		}
		*reg(ctx,(op - 0xb0)%4) = (uint16_t)arg2;

		break;
	case 0xb8:
	case 0xb9:
	case 0xba:
	case 0xbb:
	case 0xbc:
	case 0xbd:
	case 0xbe:
	case 0xbf: //mov (imm to reg) v
		printf("set %x to %x\n",op - 0xb8,*reg(ctx,op - 0xb8) = read_u16(ctx));
		break;
	case 0xe8: //call (short) v
		arg1 = (int32_t)read_u16(ctx);
		push_u16(ctx,ctx->regs.pc);
		ctx->regs.pc += arg1;
		break;
	case 0xeb: //jmp (short) b
		arg1 = (int8_t)read_u8(ctx);
		ctx->regs.pc += arg1;
		break;
	case 0xfa: //cli
		ctx->regs.eflags &= ~EFLAGS_IF;
		break;
	case 0xfb: //sti
		ctx->regs.eflags |= EFLAGS_IF;
		break;
	case 0xfc: //cld
		ctx->regs.eflags &= ~EFLAGS_DF;
		break;
	case 0xfd: //std
		ctx->regs.eflags |= EFLAGS_DF;
		break;
	default:
		error("unknow opcode %x",op);
		return -1;
	}
	return 0;
}

//return on first interrupt
int emu86(t86vm_ctx_t *ctx){
	int ret;
	while((ret = emu86i(ctx) )>= 0);
	return ret;
}
