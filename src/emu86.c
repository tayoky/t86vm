#include "t86vm.h"

//an simple 8086 emulator wrote by tayoky

#define comp_addr(seg,reg) (((seg) << 4) | (reg))

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
	return (uint8_t)emu86_read(ctx,ctx->regs.ss,(uint16_t)ctx->regs.sp++,sizeof(uint8_t));
}

static uint16_t pop_u16(t86vm_ctx_t *ctx){
	uint16_t data = (uint16_t)emu86_read(ctx,ctx->regs.ss,(uint16_t)ctx->regs.sp,sizeof(uint16_t));
	ctx->regs.sp += 2;
	return data;
}

static int16_t *reg(t86vm_ctx_t *ctx,uint8_t reg){
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

static int8_t *reg8(t86vm_ctx_t *ctx,uint8_t r){
	return ((int8_t *)reg(ctx,r%4)) + (r>=4);
}

static uint16_t *seg(t86vm_ctx_t *ctx,uint8_t seg){
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
		return &ctx->regs.fs;
	default:
		error("unknow seg %d",reg);
		return NULL;
	}
}

// 1 on reg, 0 on mem
int modrm(t86vm_ctx_t *ctx,int32_t *reg,int32_t *arg2){
	uint8_t mod = read_u8(ctx);
	*reg = (mod >> 3) & 0x7;
	uint8_t rm = mod  & 0x7;
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

	//printf("mod : %x rm : %x\n",mod,rm);

	switch(rm){
	case 0:
		*arg2 = (uint16_t)ctx->regs.bx + (uint16_t)ctx->regs.si + dis;
		break;
	case 1:
		*arg2 = (uint16_t)ctx->regs.bx + (uint16_t)ctx->regs.di + dis;
		break;
	case 2:
		*arg2 = (uint16_t)ctx->regs.bp + (uint16_t)ctx->regs.si + dis;
		break;
	case 3:
		*arg2 = (uint16_t)ctx->regs.bp + (uint16_t)ctx->regs.di + dis;
		break;
	case 4:
		*arg2 = (uint16_t)ctx->regs.si + dis;
		break;
	case 5:
		*arg2 = (uint16_t)ctx->regs.di + dis;
		break;
	case 6:
		*arg2 = (uint16_t)ctx->regs.bp + dis;
		break;
	case 7:
		*arg2 = (uint16_t)ctx->regs.bx + dis;
		break;
	}

	return 0;
}

void set_flags(t86vm_ctx_t *ctx,int32_t num){
	ctx->regs.eflags &= ~(EFLAGS_ZF | EFLAGS_PF | EFLAGS_SF);
	if(num == 0){
		ctx->regs.eflags |= EFLAGS_ZF;
	}
	if(num < 0){
		ctx->regs.eflags |= EFLAGS_SF;
	}
	//TODO : PF
}

//handle inttrtupt
void emu86_int(t86vm_ctx_t *ctx,uint8_t n){
	printf("int %hhx\n",n);
	if(n == 0x10 && *reg8(ctx,4) == 0xe){
		printf("%c\n",*reg8(ctx,0));
	}
}

//handle i/o
void emu86_out(t86vm_ctx_t *ctx,uint16_t port,uint32_t data,size_t size){
	(void)ctx;
	printf("out port : 0x%x data : 0x%x size : %zu\n",port,data,size);
}
uint32_t emu86_in(t86vm_ctx_t *ctx,uint16_t port,size_t size){
	(void)ctx;
	printf("intb port : 0x%x size : %zu\n",port,size);
	return 0;
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

	uint8_t rep = 0;

	switch(op){
	case 0xf3: //rep/repe/repz
		rep = op;
		op = read_u8(ctx);
		break;
	}
	
	int32_t arg1;
	int32_t arg2;
	int32_t arg3;
	switch(op){
	case 0x00: //add (r/m and reg) b
	case 0x01: //add (r/m and reg) v
	case 0x02: //add (reg and r/m) b
	case 0x03: //add (reg and r/m) v
	case 0x04: //add (al and imm)  b
	case 0x05: //add (ax and imm)  v
	case 0x08: //or  (r/m and reg) b
	case 0x09: //or  (r/m and reg) v
	case 0x0a: //or  (reg and r/m) b
	case 0x0b: //or  (reg and r/m) v
	case 0x0c: //or  (al and imm)  b
	case 0x0d: //or  (ax and imm)  v
	case 0x20: //and (r/m and reg) b
	case 0x21: //and (r/m and reg) v
	case 0x22: //and (reg and r/m) b
	case 0x23: //and (reg and r/m) v
	case 0x24: //and (al and imm)  b
	case 0x25: //and (ax and imm)  v
	case 0x28: //sub (r/m and reg) b
	case 0x29: //sub (r/m and reg) v
	case 0x2a: //sub (reg and r/m) b
	case 0x2b: //sub (reg and r/m) v
	case 0x2c: //sub (al and imm)  b
	case 0x2d: //sub (ax and imm)  v
	case 0x30: //xor (r/m xor reg) b
	case 0x31: //xor (r/m xor reg) v
	case 0x32: //xor (reg xor r/m) b
	case 0x33: //xor (reg xor r/m) v
	case 0x34: //xor (al xor imm)  b
	case 0x35: //xor (ax and imm)  v
	case 0x38: //cmp (r/m and reg) b
	case 0x39: //cmp (r/m and reg) v
	case 0x3a: //cmp (reg and r/m) b
	case 0x3b: //cmp (reg and r/m) v
	case 0x3c: //cmp (al and imm)  b
	case 0x3d: //cmp (ax and imm)  v
		;uint8_t isreg;
		if(op % 8 == 0x04){
			//al and imm b
			arg1 = *reg8(ctx,0);
			arg2 = read_u8(ctx);
			arg3 = 0;
			isreg = 1;
		} else if (op % 8 == 0x05){
			//ax and imm v
			arg1 = *reg(ctx,0);
			arg2 = read_u16(ctx);
			arg3 = 0;
			isreg = 1;
		} else {
			uint8_t mod = modrm(ctx,&arg1,&arg2);
			if(op % 8 >= 0x02){
				//store in reg
				isreg = 1;
				arg3 = arg1;
			} else {
				//store in r/m
				isreg = mod;
				arg3 = arg2;
			}
			if(mod){
				//r/m is reg
				if(op % 2){
					arg2 = *reg(ctx,arg2);
				} else {
					arg2 = *reg8(ctx,arg2);
				}
			} else {
				//r/m is mem address
				arg2 = emu86_read(ctx,ctx->regs.ds,arg2,(op % 2) ? sizeof(uint16_t) : sizeof(uint8_t));
			}

			if(op % 2){
				arg1 = *reg(ctx,arg1);
			} else {
				arg1 = *reg8(ctx,arg1);
			}
		}
		//FIXME : i think SF might be broken on bits ops
		//TODO : CF and OF flags
		ctx->regs.eflags &= ~(EFLAGS_SF | EFLAGS_AF | EFLAGS_SF | EFLAGS_ZF);
		switch((op/0x08)*0x08){
		case 0x00: //add
			arg1 += arg2;
			break;
		case 0x07: //or
			arg1 |= arg2;
			break;
		case 0x20: //and
			arg1 &= arg2;
			break;
		case 0x38: //cmp
		case 0x28: //sub
			//TODO : cary + overflow
			arg1 -= arg2;
			break;
		case 0x30: //xor
			error("xor");
			arg1 ^= arg2;
			break;
		}

		set_flags(ctx,arg1);

		if(op >= 0x38){
			//don't store anything for cmp
			break;
		}
		//place the result back
		if(isreg){
			if(op % 2){
				*reg(ctx,arg3) = (int16_t)arg1;
			} else {
				*reg8(ctx,arg3) = (int8_t)arg1;
			}
		} else {
			emu86_write(ctx,ctx->regs.ds,arg3,arg1,(op % 2) ? sizeof(uint16_t) : sizeof(uint8_t));
		}
		break;
	case 0x06: //push (es)
		push_u16(ctx,ctx->regs.es);
		break;
	case 0x07: //pop (es)
		ctx->regs.es = pop_u16(ctx);
		break;
	case 0x0e: //push (cs)
		push_u16(ctx,ctx->regs.cs);
		break;
		//no pop cs ???
	case 0x16: //push (ss)
		push_u16(ctx,ctx->regs.ss);
		break;
	case 0x17: //pop (ss)
		ctx->regs.ss = pop_u16(ctx);
		break;
	case 0x1e: //push (ds)
		push_u16(ctx,ctx->regs.ds);
		break;
	case 0x1f: //pop (ds)
		ctx->regs.ds = pop_u16(ctx);
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
		//TODO : AF flag
		ctx->regs.eflags &= ~(EFLAGS_SF | EFLAGS_AF | EFLAGS_SF | EFLAGS_ZF);
		printf("dec %x to %x\n",op - 0x48,--*reg(ctx,op - 0x48));
		set_flags(ctx,*reg(ctx,op-0x48));
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
	case 0x58:
	case 0x59:
	case 0x5a:
	case 0x5b:
	case 0x5c:
	case 0x5d:
	case 0x5e:
	case 0x5f: //pop (reg) v?
		*reg(ctx,op - 0x58) = pop_u16(ctx);
		break;
	case 0x70: 
	case 0x71:
	case 0x72:
	case 0x73:
	case 0x74:
	case 0x75:
	case 0x76:
	case 0x77:
	case 0x78:
	case 0x79:
	case 0x7a:
	case 0x7b: //jxx (short) b
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
		case 0x78: //js/jns
			flag = EFLAGS_SF;
			break;
		case 0x7a: //jpe/jpo
			flag = EFLAGS_PF;
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
	case 0x86: //xchg (reg an r/m) b
		if(modrm(ctx,&arg1,&arg2)){
			uint8_t tmp = *reg8(ctx,arg1);
			*reg8(ctx,arg1) = *reg8(ctx,arg2);
			*reg8(ctx,arg2) = tmp;
		} else {
			uint8_t tmp = *reg8(ctx,arg1);
			*reg8(ctx,arg1) = emu86_read(ctx,ctx->regs.ds,arg2,sizeof(uint8_t));
			emu86_write(ctx,ctx->regs.ds,arg2,tmp,sizeof(uint8_t));
		}
		break;
	case 0x87: //xchg (reg an r/m) v
		if(modrm(ctx,&arg1,&arg2)){
			uint16_t tmp = *reg(ctx,arg1);
			*reg(ctx,arg1) = *reg(ctx,arg2);
			*reg(ctx,arg2) = tmp;
		} else {
			uint16_t tmp = *reg(ctx,arg1);
			*reg(ctx,arg1) = emu86_read(ctx,ctx->regs.ds,arg2,sizeof(uint16_t));
			emu86_write(ctx,ctx->regs.ds,arg2,tmp,sizeof(uint16_t));
		}
		break;
	case 0x88: //mov (reg to r/m) b
		if(modrm(ctx,&arg1,&arg2)){
			*reg8(ctx,arg2) = *reg8(ctx,arg1);
		} else {
			emu86_write(ctx,ctx->regs.ds,arg2,*reg8(ctx,arg1),sizeof(uint8_t));
		}
		break;
	case 0x89: //mov (reg to r/m) v
		if(modrm(ctx,&arg1,&arg2)){
			*reg(ctx,arg2) = *reg(ctx,arg1);
		} else {
			emu86_write(ctx,ctx->regs.ds,arg2,*reg(ctx,arg1),sizeof(uint16_t));
		}
		break;
	case 0x8a: //mov (r/m to reg) b
		if(modrm(ctx,&arg1,&arg2)){
			*reg8(ctx,arg1) = *reg8(ctx,arg2);
		} else {
			*reg8(ctx,arg1) = emu86_read(ctx,ctx->regs.ds,arg2,sizeof(uint8_t));
		}
		break;
	case 0x8b: //mov (r/m to reg) v
		printf("mov reg to rm\n");
		if(modrm(ctx,&arg1,&arg2)){
			*reg(ctx,arg1) = *reg(ctx,arg2);
		} else {
			*reg(ctx,arg1) = emu86_read(ctx,ctx->regs.ds,arg2,sizeof(uint16_t));
		}
		break;
	case 0x8c: //mov (seg to r/m)
		if(modrm(ctx,&arg1,&arg2)){
			*reg(ctx,arg2) = *seg(ctx,arg1);
		} else {
			emu86_write(ctx,ctx->regs.ds,arg2,*seg(ctx,arg1),sizeof(uint16_t));
		}
		break;
	case 0x8d: //lea
		if(modrm(ctx,&arg1,&arg2) != 0){
			longjmp(ctx->jmperr,0);
		}
		*reg(ctx,arg1) = (uint16_t)arg2;
		break;
	case 0x8e: //mov (r/m to seg)
		if(modrm(ctx,&arg1,&arg2)){
			printf("set seg %d to reg %d\n",arg1,arg2);
			*seg(ctx,arg1) = *reg(ctx,arg2);
		} else {
			*seg(ctx,arg1) = emu86_read(ctx,ctx->regs.ds,arg2,sizeof(uint16_t));
		}
		break;
	case 0x8f: //pop (r/m) v
		if(modrm(ctx,&arg1,&arg2)){
			*reg(ctx,arg2) = pop_u16(ctx);
		} else {
			emu86_write(ctx,ctx->regs.ds,arg2,pop_u16(ctx),sizeof(uint16_t));
		}
		break;
	case 0x90: //nop
		break;
	case 0x91:
	case 0x92:
	case 0x93:
	case 0x94:
	case 0x95:
	case 0x96:
	case 0x97: //xchg (ax and reg) v
		;int16_t tmp = *reg(ctx,0);
		*reg(ctx,0) = *reg(ctx,op - 0x90);
		*reg(ctx,op - 0x90) = tmp;
		break;
	case 0xa4: //movsb
	case 0xa5: //movsw
	case 0xaa: //stosb
	case 0xab: //stosw
	case 0xac: //lodsb
	case 0xad: //lodsw
		;size_t size = op % 2 ? 2 : 1;
		size_t dis = ctx->regs.eflags& EFLAGS_DF ? -size : size;
		for(;;){
			if(rep && !ctx->regs.cx)break;

		switch(op/2*2){
		case 0xa4: //mov
			;uint32_t data = emu86_read(ctx,ctx->regs.ds,(uint16_t)ctx->regs.si,size);
			emu86_write(ctx,ctx->regs.es,(uint16_t)ctx->regs.di,data,size);
			ctx->regs.si += dis;
			ctx->regs.di += dis;
			break;
		case 0xaa: //stoxx
			emu86_write(ctx,ctx->regs.es,(uint16_t)ctx->regs.di,(uint16_t)(size == 1 ? *reg8(ctx,0) : *reg(ctx,0)),size);
			ctx->regs.di += dis;
			break;
		case 0xac: //lodxx
			if(size == 1){
				*reg8(ctx,0) = (uint16_t)emu86_read(ctx,ctx->regs.ds,(uint16_t)ctx->regs.si,size);
			} else {
				*reg(ctx,0) = (uint16_t)emu86_read(ctx,ctx->regs.ds,(uint16_t)ctx->regs.si,size);
			}
			ctx->regs.si += dis;
			break;
		}
		//TODO : check flags (repz/repnz)
		if(rep){
			//FIXME : set flags ???
			ctx->regs.cx--;
		} else {
			break;
		}
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
		*reg8(ctx,op - 0xb0) = read_u8(ctx);

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
	case 0xc5: //lds
		if(modrm(ctx,&arg1,&arg2) != 0){
			longjmp(ctx->jmperr,0);
		}
		*reg(ctx,arg1) = (uint32_t)arg2;
		error("broken : lds %d %d",arg1,arg2);
		
		break;
	case 0xc6: //mov (imm8 to r/m) b
		if(modrm(ctx,&arg1,&arg2)){
			*reg8(ctx,arg2) = read_u8(ctx);
		} else {
			emu86_write(ctx,ctx->regs.ds,arg2,read_u8(ctx),sizeof(uint8_t));
		}
		break;
	case 0xc7: //mov (imm to r/m) v
		if(modrm(ctx,&arg1,&arg2)){
			*reg(ctx,arg2) = read_u16(ctx);
		} else {
			emu86_write(ctx,ctx->regs.ds,arg2,read_u16(ctx),sizeof(uint16_t));
		}
		break;
	case 0xcc: //int (3)
		emu86_int(ctx,3);
		break;
	case 0xcd: //int (imm8)
		emu86_int(ctx,read_u8(ctx));
		break;
	case 0xe4: //in (port imm8 to al) b
		*reg8(ctx,0) = (uint16_t)emu86_in(ctx,read_u8(ctx),sizeof(uint8_t));
		break;
	case 0xe5: //in (port imm8 to ax) w
		*reg(ctx,0) = (uint16_t)emu86_in(ctx,ctx->regs.dx,sizeof(uint16_t));
		break;
	case 0xe6: //out (port imm8 to al) b
		emu86_out(ctx,read_u8(ctx),*reg8(ctx,0),sizeof(uint8_t));
		break;
	case 0xe7: //out (port imm8 to ax) w
		emu86_out(ctx,read_u8(ctx),ctx->regs.ax,sizeof(uint16_t));
		break;

	case 0xe8: //call (short) v
		arg1 = (int32_t)read_u16(ctx);
		push_u16(ctx,ctx->regs.pc);
		ctx->regs.pc += arg1;
		break;
	case 0xe9: //jmp (short) v
		arg1 = (int16_t)read_u16(ctx);
		ctx->regs.pc += arg1;
		break;
	case 0xea: //jmp (long) p
		arg1 = read_u16(ctx);
		arg2 = read_u16(ctx);
		ctx->regs.pc = (uint16_t)arg1;
		ctx->regs.cs = (uint16_t)arg2;
		break;
	case 0xeb: //jmp (short) b
		arg1 = (int8_t)read_u8(ctx);
		ctx->regs.pc += arg1;
		break;
	case 0xec: //in (port dx to al) b
		*reg8(ctx,0) = (uint16_t)emu86_in(ctx,ctx->regs.dx,sizeof(uint8_t));
		break;
	case 0xed: //in (port dx to ax) w
		*reg(ctx,0) = (uint16_t)emu86_in(ctx,ctx->regs.dx,sizeof(uint16_t));
		break;
	case 0xee: //out (port dx to al) b
		emu86_out(ctx,ctx->regs.dx,*reg8(ctx,0),sizeof(uint8_t));
		break;
	case 0xef: //out (port dx to ax) w
		emu86_out(ctx,ctx->regs.dx,ctx->regs.ax,sizeof(uint16_t));
		break;
	case 0xf8: //clc
		ctx->regs.eflags &= ~EFLAGS_CF;
		break;
	case 0xf9: //stc
		ctx->regs.eflags |= EFLAGS_CF;
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
	printf("ax = %hx\n",ctx->regs.ax);
	printf("bx = %hx\n",ctx->regs.bx);
	printf("cx = %hx\n",ctx->regs.cx);
	printf("dx = %hx\n",ctx->regs.dx);
	printf("sp = %hx\n",ctx->regs.sp);
	printf("bp = %hx\n",ctx->regs.bp);
	printf("si = %hx\n",ctx->regs.si);
	printf("di = %hx\n",ctx->regs.di);
	printf("es = %hx\n",ctx->regs.es);
	printf("cs = %hx\n",ctx->regs.cs);
	printf("ds = %hx\n",ctx->regs.ds);
	printf("ss = %hx\n",ctx->regs.ss);
	printf("fs = %hx\n",ctx->regs.fs);
	printf("pc = %hx\n",ctx->regs.pc);
	printf("eflags = %hx\n",ctx->regs.eflags);
}

int emu86(t86vm_ctx_t *ctx){
	int ret;
	while((ret = emu86i(ctx) )>= 0);
	emu86_dump(ctx);
	return ret;
}
