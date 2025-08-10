#include <stdio.h>
#include <stdarg.h>
#include "t86vm.h"

void error(const char *fmt,...){
	va_list args;
	va_start(args,fmt);
	fputs("t86vm : ",stderr);
	vfprintf(stderr,fmt,args);
	fputc('\n',stderr);
	va_end(args);
}
