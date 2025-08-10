#include <string.h>
#include <stdio.h>
#include "t86vm.h"

int emul(t86vm_ctx_t *ctx){
	if(ctx->ram_size < 0x7c00 + 512){
		error("not enought ram");
		return -1;
	}
	//read and load boot sector of floppy
	//at 0x7c00
	char boot[512];
	if(!fread(boot,sizeof(boot),1,ctx->floppy)){
		perror("read");
		return -1;
	}
	memcpy(&ctx->ram[0x7c00],boot,sizeof(boot));

	//now launch the floppy disk
	ctx->regs.pc = 0x7c00;
	printf("%d\n",emu86(ctx));
	return 0;
}
