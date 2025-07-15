#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "rtree.h"

#define REPEAT 2000
#define ITERATION 5000

typedef struct metadata {
	uint64_t addr;
} md_t;

int 
main()
{
	rtree *r = malloc(sizeof(rtree));
	uint64_t addr;
	md_t md[ITERATION];

	if (rtree_init(r) != 0) {
		printf("Failed to init rtree.\n");
		exit(0);
	}

	srand(time(NULL)); 

	for (int rep=0;rep<REPEAT;rep++) {
		printf("Repeat %d\n", rep);
		for (int i=0;i<ITERATION;i++) {
			while (1) {
				/* I know this is 32bit, but anyways... */
				md[i].addr = rand();
				if (md[i].addr != 0 && !rtree_find(r, (void *)md[i].addr))
					break;
			}
			rtree_insert(r, (void *)md[i].addr, &md[i]);
		}

		for (int i=0;i<ITERATION;i++) {
			addr = (uint64_t)rtree_find(r, (void *)md[i].addr);
			assert(addr);
			assert(((md_t *)addr)->addr == md[i].addr);
		}

		for (int i=0;i<ITERATION;i++) {
			rtree_delete(r, (void *)md[i].addr);
		}
	}

	printf("pass.\n");

	rtree_destroy(r);
	return 0;
}
    
