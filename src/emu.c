#include <string.h>
#include <stdio.h>
#include "t86vm.h"

int emul(t86vm_ctx_t *ctx){
	//automatic bios address based on size
	ctx->bios->base = 0x100000 - ctx->bios->size;
	cpu8086_reset(&ctx->cpu);
	printf("%d\n",cpu8086_emu(ctx));
	return 0;
}
