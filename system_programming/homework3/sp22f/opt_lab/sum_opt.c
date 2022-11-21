#include<stdio.h>
#include<stdlib.h>
#include<assert.h>
#include<limits.h>

#define MAX_NUM 100000000
//#define MAX_NUM 1000000000
//#define MAX_NUM INT_MAX
//#define MAX_NUM 100000

struct vec {
	int len;
	int *data;
};
typedef struct vec vec_t;

int* get_vec_start(vec_t* v)
{
	return v->data;
}

void combine(vec_t* v, int *dest)
{
	assert(dest);
	int v_len = v->len;
	int *d = get_vec_start(v);
	int t = 0;
	for(int i = 0; i < v_len; i++)
		t = t + d[i];
	*dest = t;
}

void init(vec_t* v)
{
	assert(v);
	v->len = MAX_NUM;
	v->data = (int*) malloc(sizeof(int) * MAX_NUM);

	// init 
	for(int i = 0; i < MAX_NUM; i++)
		v->data[i] = i;
}

int main()
{
	printf("This is the naive version .. \n");

	vec_t info; 
	int result = 0; 
	// init 
	init(&info);

	// combine 
	combine(&info, &result);
	printf("combined val = %d\n", result);

	return 0;
}
