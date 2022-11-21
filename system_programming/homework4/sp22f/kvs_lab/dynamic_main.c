#include "kvs.h"
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

int main()
{
	kvs_t*(*open)();
	int (*put)(kvs_t*, const char*, const char*);
	char* (*get)(kvs_t*, const char*);
	int (*seek)(kvs_t*);
	int (*close)(kvs_t*);

	char *error;
	void *handle;
	handle = dlopen("./libkvs.so", RTLD_LAZY);
	if (!handle){
		fprintf(stderr, "%s\n", dlerror());
		exit(1);
	}
	open = (kvs_t*(*)()) dlsym(handle, "open");
	put = (int(*)(kvs_t*, const char*, const char*)) dlsym(handle, "put");
	get = (char*(*)(kvs_t*, const char*)) dlsym(handle, "get");
	seek = (int(*)(kvs_t*)) dlsym(handle, "seek");
	close = (int(*)(kvs_t*)) dlsym(handle, "close");

	if((error = dlerror()) != NULL){
		fprintf(stderr, "%s\n", error);
		exit(1);
	}

	// 1. create KVS
	kvs_t* kvs = open();
	
	if(!kvs){
		printf("Failed to open kvs\n");
		return -1;
	}
	
	char key_sub[100];
	char* value_sub = (char*) malloc (sizeof(char)* 100);
	FILE* pFile_put = fopen("student.dat", "r");
	
	if (pFile_put == NULL)
		fprintf(stderr, "File Open Error!\n");
	printf("Put operation ...\n");
	
	while(!feof(pFile_put)){
		if (fscanf(pFile_put, "%s %s", key_sub, value_sub) == 2){
			put(kvs, key_sub, value_sub);
		}
	}
	
	fclose(pFile_put);
		
	char key[100]; 
	char* value = (char*) malloc (sizeof(char)* 100);
	char* rvalue;
	FILE* pFile_get = fopen("student.dat", "r");
	
	printf("\nGet operation ...\n");

	if (pFile_get == NULL)
		fprintf(stderr, "File Open Error!\n");
	while(!feof(pFile_get)){
		if (fscanf(pFile_get, "%s %s", key, value) == 2){
			rvalue = get(kvs, key);
			if(!rvalue){
				printf("Failed to get data\n");
				exit(-1);
			}
			else
				printf("get: %s %s\n", key, rvalue);
		}
	}

	fclose(pFile_get);

	printf("\nSeek operation ...\n");
	seek(kvs);


	// 5. close 
	close(kvs);
	
	return 0;
}
