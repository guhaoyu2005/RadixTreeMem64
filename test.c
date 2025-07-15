#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "rtree.h"

#define ITERATION 50000

typedef struct metadata {
	uint64_t addr;
} md_t;

int 
main()
{
	uint64_t addr;
	md_t md[ITERATION];

	srand(time(NULL)); 

	printf("inserting..\n");
	for (int i=0;i<ITERATION;i++) {
		while (1) {
			md[i].addr = rand();
			if (md[i].addr != 0 && !rtree_find((void *)md[i].addr))
				break;
		}
		rtree_insert((void *)md[i].addr, &md[i]);
	}

	printf("checking..\n");
	for (int i=0;i<ITERATION;i++) {
		addr = (uint64_t)rtree_find((void *)md[i].addr);
		assert(addr);
		assert(((md_t *)addr)->addr == md[i].addr);
	}
	
	printf("pass.\n");

	rtree_destroy();
	return 0;
}
    
