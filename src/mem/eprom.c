#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "t86vm.h"

eprom_t *eprom_open(const char *path){
	FILE *file = fopen(path,"r");
	if(!file)return NULL;

	eprom_t *eprom = malloc(sizeof(eprom_t));
	if(!eprom)return NULL;
	memset(eprom,0,sizeof(eprom_t));
	
	fseek(file,0,SEEK_END);
	eprom->size = ftell(file);
	
	rewind(file);
	eprom->data = malloc(eprom->size);
	fread(eprom->data,eprom->size,1,file);
	
	fclose(file);
	return eprom;
}

uint32_t eprom_read(eprom_t *eprom,uint32_t addr,size_t size,int *error){
	//FIXME : do we need bound check here ?
	uint32_t data = 0;

	for(size_t i=0; i<size; i++){
		data |= ((uint8_t)eprom->data[addr - eprom->base + i]) << (i * 8);
	}

	return data;
}
