#include "t86vm.h"

//an simple 8086 emulator wrote by tayoky

#define comp_addr(seg,reg) (((seg) << 4) | reg)

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

void emu86_write(t86vm_ctx_t *ctx,uint16_t seg,uint32_t addr,uint32_t data,size_t size){
	uint32_t a = comp_addr(ctx->seg_overide >= 0 ? ctx->seg_overide : seg,addr);

	if(a >= ctx->ram_size || a % size){
		longjmp(ctx->jmperr,0);
	}

	for(size_t i=0; i<size; i++){
		ctx->ram[a + i] = (uint8_t)(data >> (i * 8));
	}
}

uint32_t emu86_read(t86vm_ctx_t *ctx,uint16_t seg,uint32_t addr,size_t size){
	uint32_t a = comp_addr(ctx->seg_overide >= 0 ? ctx->seg_overide : seg,addr);

	if(a >= ctx->ram_size || a % size){
		longjmp(ctx->jmperr,0);
	}

	uint32_t data = 0;

	for(size_t i=0; i<size; i++){
		data |= ((uint8_t)ctx->ram[a + i]) << (i * 8);
	}

	return data;
}


static void push_u8(t86vm_ctx_t *ctx,uint8_t data){
	emu86_write(ctx,ctx->regs.ss,--ctx->regs.sp,data,sizeof(data));
}

static void push_u16(t86vm_ctx_t *ctx,uint16_t data){
	emu86_write(ctx,ctx->regs.ss,ctx->regs.sp-=2,data,sizeof(data));
}

static uint8_t pop_u8(t86vm_ctx_t *ctx){
	return (uint8_t)emu86_read(ctx,ctx->regs.ss,ctx->regs.sp++,sizeof(uint8_t));
}

static uint16_t pop_u16(t86vm_ctx_t *ctx){
	uint16_t data = (uint16_t)emu86_read(ctx,ctx->regs.ss,ctx->regs.sp,sizeof(uint16_t));
	ctx->regs.sp += 2;
	return data;
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

// 1 on reg, 0 on mem
int modrm(t86vm_ctx_t *ctx,int32_t *reg,int32_t *arg2){
	uint8_t mod = read_u8(ctx);
	*reg = mod & 0x7;
	uint8_t rm = (mod >> 3) & 0x7;
	mod = (mod >> 6) & 0x03;

	if(mod == 3){
		//rm is an reg
		*arg2 = rm;
		return 1;
	}

	//special case for mod = 00 rm = 6
	if(mod == 0 && rm == 6){
		//16 bits memory address
		*arg2 = read_u16(ctx);
		return 0;
	}

	int16_t dis;
	switch(mod){
	case 0b00:
		dis= 0;
		break;
	case 0b01:
		dis = (int8_t)read_u8(ctx);
		break;
	case 0b10:
		dis = (int16_t)read_u16(ctx);
		break;
	}

	switch(rm){
	case 0:
		*arg2 = ctx->regs.bx + ctx->regs.si + dis;
		break;
	case 1:
		*arg2 = ctx->regs.bx + ctx->regs.di + dis;
		break;
	case 2:
		*arg2 = ctx->regs.bp + ctx->regs.si + dis;
		break;
	case 3:
		*arg2 = ctx->regs.bp + ctx->regs.di + dis;
		break;
	case 4:
		*arg2 = ctx->regs.si + dis;
		break;
	case 5:
		*arg2 = ctx->regs.di + dis;
		break;
	case 6:
		*arg2 = ctx->regs.bp + dis;
		break;
	case 7:
		*arg2 = ctx->regs.bx + dis;
		break;
	}

	return 0;
}

void set_flags(t86vm_ctx_t *ctx,uint32_t num){
	ctx->regs.eflags &= ~(EFLAGS_ZF | EFLAGS_PF);
	if(num == 0){
		ctx->regs.eflags |= EFLAGS_ZF;
	}
	//TODO : PF
}

int emu86i(t86vm_ctx_t *ctx){

	if(setjmp(ctx->jmperr)){
		//TODO : int ??
		error("VM error");
		return -1;
	}

	uint8_t op = read_u8(ctx);
	ctx->seg_overide = -1;

	//seg overide
	switch(op){
	case 0x26:
		ctx->seg_overide = ctx->regs.es;
		op = read_u8(ctx);
		break;
	case 0x2e:
		ctx->seg_overide = ctx->regs.cs;
		op = read_u8(ctx);
		break;
	case 0x36:
		ctx->seg_overide = ctx->regs.ss;
		op = read_u8(ctx);
		break;
	case 0x3e:
		ctx->seg_overide = ctx->regs.ds;
		op = read_u8(ctx);
		break;
	}
	
	int32_t arg1;
	int32_t arg2;
	switch(op){
	case 0x06: //push (es)
		push_u16(ctx,ctx->regs.es);
		break;
	case 0x07: //pop (es)
		ctx->regs.es = pop_u16(ctx);
		break;
	case 0x16: //push (ss)
		push_u16(ctx,ctx->regs.ss);
		break;
	case 0x17: //pop (ss)
		ctx->regs.ss = pop_u16(ctx);
		break;
	case 0x31:
	case 0x33: //xor
		//TODO : diff between 31 and 33 ????
		if(modrm(ctx,&arg1,&arg2) != 1){
			longjmp(ctx->jmperr,0);
		}
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
		ctx->regs.eflags &= ~(EFLAGS_SF | EFLAGS_AF | EFLAGS_SF | EFLAGS_ZF);
		if((*reg(ctx,op - 0x40) & 0xf) == 0xf)ctx->regs.eflags |= EFLAGS_AF;
		printf("inc %x to %x\n",op - 0x40,++*reg(ctx,op - 0x40));
		set_flags(ctx,*reg(ctx,op - 0x40));
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
		ctx->regs.eflags &= ~(EFLAGS_SF | EFLAGS_AF | EFLAGS_SF | EFLAGS_ZF);
		printf("dec %x to %x\n",op - 0x48,--*reg(ctx,op - 0x48));
		break;
	case 0x50:
	case 0x51:
	case 0x52:
	case 0x53:
	case 0x54:
	case 0x55:
	case 0x56:
	case 0x57: //push (reg) v?
		push_u16(ctx,*reg(ctx,op -0x50));
		break;
	case 0x70: 
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77://jxx (short) b
		arg1 = (int8_t)read_u8(ctx);
		uint16_t flag;
		switch(op % 2){
		case 0x70: //jo/jno
			flag = EFLAGS_OF;
			break;
		case 0x72: //jb/jnb
			flag = EFLAGS_CF;
			break;
		case 0x74: //jz/jnz
			flag = EFLAGS_ZF;
			break;
		case 0x76: //jbe/ja
			flag = EFLAGS_CF | EFLAGS_ZF;
			break;
		}
		if(op % 2){
			if(!(ctx->regs.eflags & flag)){
		ctx->regs.pc += arg1;
			}
		} else {
			if(ctx->regs.eflags & flag){
		ctx->regs.pc += arg1;
			}
		}
		break;
	case 0x8e: //mov (reg to seg)
		if(modrm(ctx,&arg1,&arg2) != 1){
			longjmp(ctx->jmperr,0);
		}
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
	case 0xc5:
		if(modrm(ctx,&arg1,&arg2) != 0){
			longjmp(ctx->jmperr,0);
		}
		error("broken : lds %d %d",arg1,arg2);
		
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

void emu86_dump(t86vm_ctx_t *ctx){
	puts("dump :");
	printf("ax = %x\n",ctx->regs.ax);
	printf("bx = %x\n",ctx->regs.bx);
	printf("cx = %x\n",ctx->regs.cx);
	printf("dx = %x\n",ctx->regs.dx);
	printf("sp = %x\n",ctx->regs.sp);
	printf("bp = %x\n",ctx->regs.bp);
	printf("si = %x\n",ctx->regs.si);
	printf("di = %x\n",ctx->regs.di);
	printf("es = %x\n",ctx->regs.es);
	printf("cs = %x\n",ctx->regs.cs);
	printf("ds = %x\n",ctx->regs.ds);
	printf("ss = %x\n",ctx->regs.ss);
	printf("pc = %x\n",ctx->regs.pc);
	printf("eflags = %x\n",ctx->regs.eflags);
}

//return on first interrupt
int emu86(t86vm_ctx_t *ctx){
	int ret;
	while((ret = emu86i(ctx) )>= 0);
	emu86_dump(ctx);
	return ret;
}
