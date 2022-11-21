#include "kvs.h"

int close(kvs_t* kvs)
{
	free(kvs);
	return 0;
}
