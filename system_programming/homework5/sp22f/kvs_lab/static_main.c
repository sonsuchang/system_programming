#include "kvs.h"

int main()
{
	// 1. create KVS
	kvs_t* kvs = open();

	if(!kvs) {
		printf("Failed to open kvs\n");
		return -1;
	}

	// 2. put data 
	
	// 1) file read 
	// 2) put data 

#if 0
	char key[100]; 
	char* value = (char*) malloc (sizeof(char)* 300);
	char* rvalue;

	strcpy(key, "Eunji");
	strcpy(value, "Seoul");

	if(put(kvs, key, value) < 0){
		printf("Failed to put data\n");
		exit(-1);
	}

#endif


	// 3. get for test 

	// 1) file read 
	// 2) get & compare return value with original vallue 

#if 0
	if(!(rvalue = get(kvs, key))){
		printf("Failed to get data\n");
		exit(-1);
	}

	printf("get: %s, %s\n", key, rvalue);
#endif

	// 4. print all items 
#if 0
	int nitems = seek(kvs);
	printf("%d items are found\n", nitems);


	// 5. close 
	close(kvs);
#endif
	
	return 0;
}
