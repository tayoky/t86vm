#include <setjmp.h>
#include "t86vm.h"

#define comp_addr(seg,reg) (((seg) << 4) | (reg))

void mem_write(t86vm_ctx_t *ctx,uint16_t seg,uint32_t addr,uint32_t data,size_t size){
	uint32_t a = comp_addr(ctx->seg_overide >= 0 ? ctx->seg_overide : seg,addr);

	if(a >= ctx->ram_size){
		longjmp(ctx->jmperr,0);
	}

	for(size_t i=0; i<size; i++){
		ctx->ram[a + i] = (uint8_t)(data >> (i * 8));
	}
}

uint32_t mem_read(t86vm_ctx_t *ctx,uint16_t seg,uint32_t addr,size_t size){
	uint32_t a = comp_addr(ctx->seg_overide >= 0 ? ctx->seg_overide : seg,addr);

	if(a >= ctx->ram_size){
		longjmp(ctx->jmperr,0);
	}

	uint32_t data = 0;

	for(size_t i=0; i<size; i++){
		data |= ((uint8_t)ctx->ram[a + i]) << (i * 8);
	}

	return data;
}
