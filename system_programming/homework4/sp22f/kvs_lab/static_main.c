#include "kvs.h"
#include <stdio.h>
#include <string.h>

int main()
{
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
