#include "kvs.h"
#include <stdio.h>
#include <string.h>

int put(kvs_t* kvs, const char* key, const char* value)
{
	node_t *new_node = (node_t*)malloc(sizeof(node_t));
	node_t *search_node = kvs->db;
	strcpy(new_node->key, key);
	new_node->value = (char*) malloc (sizeof(char) * 100);
	strcpy(new_node->value, value);
	new_node->next = NULL;
	
	if(kvs->items == 0){
		kvs->db = new_node;
	}

	else{
		while(search_node->next != NULL){
			search_node = search_node->next;
		}
		search_node->next = new_node;
	}	
	printf("put: %s %s\n", new_node->key, new_node->value);
	kvs->items += 1;
	return 0;
}
