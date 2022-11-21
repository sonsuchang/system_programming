#include "kvs.h"
#include <stdio.h>
#include <string.h>

char* get(kvs_t* kvs, const char* key)
{
	char* value = (char*)malloc(sizeof(char)*100);
	node_t *temp = kvs->db;

	if(!value){
		printf("Failed to malloc\n");
		return NULL;
	}
	
	while(temp != NULL){
		if (strcmp(key, temp->key) == 0)
		{
			strcpy(value, temp->value);
			break;
		}
		else{
			temp = temp->next;
		}
		
	}
	return value;
}
