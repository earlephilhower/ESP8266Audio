
// #include "esp_common.h"

char unalChar(const char *adr) {
	int *p=(int *)((int)adr&0xfffffffc);
	int v=*p;
	int w=((int)adr&3);
	if (w==0) return ((v>>0)&0xff);
	if (w==1) return ((v>>8)&0xff);
	if (w==2) return ((v>>16)&0xff);
	if (w==3) return ((v>>24)&0xff);
}


short unalShort(const short *adr) {
	int *p=(int *)((int)adr&0xfffffffc);
	int v=*p;
	int w=((int)adr&3);
	if (w==0) return (v&0xffff); else return (v>>16);
}
