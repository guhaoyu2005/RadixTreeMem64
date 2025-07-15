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

void
callback_delete(void *metadata)
{
	md_t *m = (md_t *)metadata;
	//printf("free callback: %lu\n", m->addr); 
}

int
find_le(md_t *arr, int n, uint64_t target)
{
#define DIST(a, b) (a > b ? (a-b): (b-a))
	uint64_t closest = (uint64_t)-1;

	for (int i=0;i<n;i++) {
		if (arr[i].addr < target && 
		    (DIST(arr[i].addr, target) < DIST(target, closest))) {
			closest = arr[i].addr;
		}
	}

	return closest;
}

int 
main()
{
	rtree *r = malloc(sizeof(rtree));
	uint64_t addr;
	md_t md[ITERATION];

	goto findle_test;

	if (rtree_init(r) != 0) {
		printf("Failed to init rtree.\n");
		exit(0);
	}

	r->rtree_cb_delete = &callback_delete;

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
	free(r);

findle_test:
	// Get a clean rtree and test find_le
	if (rtree_init(r) != 0) {
		printf("Failed to init rtree.\n");
		exit(0);
	}
	
	for (int i=0;i<ITERATION;i++) {
		md[i].addr = rand();//i+1;
		rtree_insert(r, (void *)md[i].addr, &md[i]);
	}

	for (int i=2;i<ITERATION;i+=2) {
		printf("Deleting %lu\n", md[i].addr);
		rtree_delete(r, (void *)md[i].addr);
		addr = (uint64_t)rtree_find_le(r, (void *)md[i].addr);
		assert(addr);
		printf("found %lu\n", ((md_t *)addr)->addr);
		assert(((md_t *)addr)->addr == find_le(md, ITERATION, md[i].addr));
	}
	
	printf("find_le pass.\n");

	rtree_destroy(r);
	free(r);

	return 0;
}
    
