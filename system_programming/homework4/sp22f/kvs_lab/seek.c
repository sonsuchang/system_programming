#include "kvs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int compare(const void *a, const void *b);

int seek(kvs_t* kvs)
{
	int count = 0;
	node_t *temp = kvs->db;
	char* key_array[kvs->items];
	for (int i = 0; i < kvs->items; i++){
		//printf("%s\n", temp->key);
		key_array[i] = temp->key;
		temp = temp->next;
	}

	qsort(key_array, kvs->items, sizeof(char*), compare);
	
	while(1){
		if (count == kvs->items)
			break;
		node_t *search_key = kvs->db;
		while(search_key != NULL)
		{
			if (strcmp(key_array[count], search_key->key) == 0){
				printf("(%s, %s)\n", search_key->key, search_key->value);
				count++;
				break;
			}
			else
				search_key = search_key->next;
		}
	}
	
	return 0;
}
static int compare(const void *a, const void *b){
	return (strcmp(*(char**)a, *(char**)b));
}
