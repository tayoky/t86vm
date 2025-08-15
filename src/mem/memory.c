#include "t86vm.h"

void mem_write(t86vm_ctx_t *ctx,uint32_t addr,uint32_t data,size_t size,int *error){
	*error = 0;
	if(addr >= ctx->ram_size){
		*error = 1;
	}

	for(size_t i=0; i<size; i++){
		ctx->ram[addr + i] = (uint8_t)(data >> (i * 8));
	}
}

uint32_t mem_read(t86vm_ctx_t *ctx,uint32_t addr,size_t size,int *error){
	*error = 0;

	if(addr >= ctx->bios->base && addr < ctx->bios->base + ctx->bios->size){
		return eprom_read(ctx->bios,addr,size,error);
	}
	if(addr >= ctx->ram_size){
		*error = 1;
	}

	uint32_t data = 0;

	for(size_t i=0; i<size; i++){
		data |= ((uint8_t)ctx->ram[addr + i]) << (i * 8);
	}

	return data;
}
