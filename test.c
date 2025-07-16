#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "rtree.h"

#define REPEAT 2000
#define ITERATION 5000

typedef struct metadata {
	uint64_t addr;
	int deleted;
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
		    (arr[i].deleted == 0) &&
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
	uint64_t addr, verify;
	md_t md[ITERATION];

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
				md[i].addr = rand() + 1;
				md[i].deleted = 0;
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
			md[i].deleted = 0;
			rtree_delete(r, (void *)md[i].addr);
		}
	}

	printf("pass.\n");

	rtree_destroy(r);
	free(r);

find_le_test:
	memset(md, 0, sizeof(md));
	// Get a clean rtree and test find_le
	if (rtree_init(r) != 0) {
		printf("Failed to init rtree.\n");
		exit(0);
	}
	
	for (int rep=0;rep<REPEAT;rep++) {
		printf("find_le Repeat %d\n", rep);
		for (int i=0;i<ITERATION;i++) {
			assert(md[i].deleted == 0);
			while (1) {
				/* I know this is 32bit, but anyways... */
				md[i].addr = rand() + 1;
				if (md[i].addr != 0 && !rtree_find(r, (void *)md[i].addr)) {
					for (int j=0;j<i;j++) {
						assert(md[j].addr != md[i].addr);
					}
					break;
				}
			}
			rtree_insert(r, (void *)md[i].addr, &md[i]);
		}
	
		for (int i=2;i<ITERATION;i+=2) {
			rtree_delete(r, (void *)md[i].addr);
			md[i].deleted = 1;

			addr = (uint64_t)rtree_find_le(r, (void *)md[i].addr);
			verify = find_le(md, ITERATION, md[i].addr);
			if (verify != (uint64_t)-1) {
				if (addr == NULL) {
					printf("orig %lu, verify: %lu\n", 
					    md[i].addr, verify);
					assert(0);
				}
				assert(((md_t *)addr)->addr == verify);
			} else {
				if (addr == NULL) {
					continue;
				}
			}
		}

		for (int i=0;i<ITERATION;i++) {
			if (md[i].deleted) {
				md[i].deleted = 0;
				continue;
			}
			md[i].deleted = 0;
			rtree_delete(r, (void *)md[i].addr);
		}
	}
	
	printf("find_le pass.\n");

	rtree_destroy(r);
	free(r);

	return 0;
}
    
