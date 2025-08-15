#include <string.h>
#include <stdio.h>
#include "t86vm.h"

int emul(t86vm_ctx_t *ctx){
	//i belive the bios is loaded at 0xf0000
	ctx->bios->base = 0xf0000;
	cpu8086_reset(&ctx->cpu);
	printf("%d\n",cpu8086_emu(ctx));
	return 0;
}
