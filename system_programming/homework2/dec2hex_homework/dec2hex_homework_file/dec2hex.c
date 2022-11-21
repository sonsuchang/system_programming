#include<stdio.h>
#include<stdbool.h>
#include<stdlib.h>

typedef unsigned char* pointer;

bool is64bit()
{
	if (sizeof(long) == 8)
		return true;
	else
		return false;
}

bool isBigEndian(){
	int i = 0x00000001;
	if(((char *)&i)[0])
		return false;
	else
		return true;
}

void le_show_bytes(pointer start, size_t len)
{
	size_t i;
	for (i = 1; i < len + 1; i++)
		printf("%.2X", start[len - i]);
	printf("\n");
	return;
}

void be_show_bytes(pointer start, size_t len){
	size_t i;
	for (i = 0; i < len; i++)
		printf("%.2X", start[i]);
	printf("\n");
	return;
}


int main(int argc, char* argv[])
{
	if(argc < 2){
		printf("Usage: ./a.out number\n");
		exit(0);
	}
	unsigned int a = atoi(argv[1]);

	printf("ARCH=%d\n", is64bit()? 64 : 32);
	printf("ORDERING=%s\n", isBigEndian()? "BIG_ENDIAN": "LITTLE_ENDIAN");

	printf("MYANS: DEC=%d HEX=", a);
	isBigEndian()? 
		be_show_bytes((pointer) &a, sizeof(unsigned int)): le_show_bytes((pointer) &a, sizeof(unsigned int));
		//be_show_bytes((pointer)&a, sizeof(unsigned int)): le_show_bytes((pointer)(&a), sizeof(unsigned int));

	printf("CHECK: DEC=%d HEX=%.8X\n", a, a);
	return 0;
}
