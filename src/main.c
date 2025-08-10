#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "t86vm.h"

int main(int argc,char **argv){
	int c;
	t86vm_ctx_t ctx;
	memset(&ctx,0,sizeof(ctx));
	ctx.ram_size = 1024 * 1024;
	while((c = getopt(argc,argv,"mf:")) != -1){
		switch(c){
		case '?':
			return 1;
		case 'm':
			break;
		case 'f':
			ctx.floppy = fopen(optarg,"r+");
			if(!ctx.floppy){
				perror(optarg);
				return 1;
			}
			break;
		}
	}

	ctx.ram = malloc(ctx.ram_size);
	
	return emul(&ctx) < 0 ? 1 : 0;
}
