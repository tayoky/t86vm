#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t86vm.h"

#define OPT(str) !strcmp(argv[i],str)

int main(int argc,char **argv){
	t86vm_ctx_t ctx;
	memset(&ctx,0,sizeof(ctx));
	ctx.ram_size = 1024 * 512;
	for(int i=1; i<argc; i++){
		if(OPT("-f")  || OPT("--floppy")){
			i++;
			if(i>=argc)goto missing_operand;
			ctx.floppy = fopen(argv[i],"r+");
			if(!ctx.floppy){
				perror(argv[i]);
				return 1;
			}
			continue;
		}
		if(OPT("-m") || OPT("--memory")){
			i++;
			if(i>=argc)goto missing_operand;
			ctx.ram_size = strtoul(argv[i],NULL,0);
			continue;
		}
		if(OPT("-b") || OPT("--bios")){
			if(ctx.bios){
				error("cannot add multiples bios");
				return 1;
			}
			i++;
			if(i>=argc)goto missing_operand;
			ctx.bios = eprom_open(argv[i]);
			if(!ctx.bios){
				perror(argv[i]);
				return 1;
			}
			continue;
		}
		error("unknow option : %s",argv[i]);
		return 1;
missing_operand:
		error("missing operand");
		return 1;
	}

	ctx.ram = malloc(ctx.ram_size);
	memset(ctx.ram,0,ctx.ram_size);
	if(!ctx.bios)ctx.bios = eprom_open("bios.rom");
	if(!ctx.bios){
		perror("cannot open default bios");
		return 1;
	}
	
	return emul(&ctx) < 0 ? 1 : 0;
}
