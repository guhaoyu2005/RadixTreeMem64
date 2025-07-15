/*
Copyright (c) 2025 Haoyu Gu 

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "rtree.h"

#ifndef RTREE_CUSTOM_ALLOCATION
#include <stdlib.h>

#define RTMALLOC malloc
#define RTFREE free
#endif

#define u8 uint8_t
#define u64 uint64_t

#define MEMBITS 64

#ifdef RTREE_DEBUG
void
PB(u64 n) 
{
	for (int i=MEMBITS-1; i>=0; i--) {
		printf("%d", (((((u64)1)<<(u64)i) & n) > 0));
	}
}
#endif

typedef struct rtree_node node_t;

struct rtree_node {
	// Address bits at current node
	u64 key;
	// Starting bit index in original address
	u8 bit;
	// Address bits length in key.
	// E.g. bit_len = 4, key(in hex) = 10 => [0010]
	u8 bit_len;
	u8 __pad[6];
	node_t *parent;
	// Binary tree: we are using addr in binary format so children starts
	// with either zero or one.
	node_t *children[2];
	// metadata (last node)
	void *metadata;
};

node_t rtree_root = {
    .key = 0,
    .bit = 0,
    .bit_len = 64,
    .parent = NULL,
    .children = { NULL },
    .metadata = NULL
};

#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
#define MASK64(B, L) ( (B) == 0 ? \
    ( ((u64)(-1)) - ((L)>0 ? (((u64)1) << (MEMBITS - (B) - (L))) - 1 : 0) ) : \
    ( L>0 ? \
      ((((u64)1) << (MEMBITS - (B))) - 1 - ((((u64)1) << (MEMBITS - (B) - (L))) - 1)) : \
      ((u64)-1) ))
#define GETBIT(X, N) (((X) & (((u64)1) << (MEMBITS - (N) - 1))) > 0)



int
rtree_insert(void *p, void *metadata)
{	
	node_t *cn = &rtree_root, *sn;
	u64 ip = (u64)p, pp, x, k;
	u8 bit = 0, child_idx, l, yl, b, d;
	void *md;

	while (cn) {
		// Check partial address with stored key
		pp = ip & MASK64(bit, cn->bit_len);
		//pp = (ip >> (MEMBITS - cn->bit_len - bit)) & ((1 << cn->bit_len) - 1);
		// XOR them
		x = pp ^ cn->key;
		// If they are the same, then insert to the child
		// Or if this is the last part of address, update the metadata?
		if (!x) {
			if (cn->bit_len + bit == MEMBITS) {
				// TODO: update metadata???
				//assert(0);
				printf("DUPLICATED\n");
				return 0;
			}
			
			bit += cn->bit_len;
			child_idx = GETBIT(ip, bit);
			cn = cn->children[child_idx];
			continue;
		}

		// Not the same? Split child
		// which bit if different
		d = LOG2(x);
		yl = d - (MEMBITS - (cn->bit + cn->bit_len) - 1); 
		child_idx = GETBIT(pp, (MEMBITS - d - 1));

		// If the difference falls in children
		if (yl >= cn->bit_len) {
			bit += cn->bit_len;
			cn = cn->children[child_idx];
			continue;
		}
		
		// save info for node splitted from cn  
		b = MEMBITS - d - 1;
		l = d + 1;
		k = (cn->key & (((u64)1 << l) - 1)); 
		md = cn->metadata;

		// update current node
		cn->bit_len -= yl;
		cn->key &= MASK64(cn->bit, cn->bit_len);

		// Create cn splitted node
		sn = RTMALLOC(sizeof(node_t));
		if (sn == NULL) {
			return -1;
		}
		sn->key = k;
		sn->bit = b;
		sn->bit_len = yl;
		sn->parent = cn;
		sn->children[0] = cn->children[0];
		sn->children[1] = cn->children[1];
		sn->metadata = md;
		assert(l != 0);

		// Insert cn splitted node
		cn->children[child_idx ^ 1] = sn; 
		cn->children[child_idx] = NULL;
		cn->metadata = NULL;

		// update bit
		bit += cn->bit_len;

		// Check if we need to go deeper
		if (cn->children[child_idx] == NULL) {
			break;
		} else {
			cn = cn->children[child_idx];
			continue;
		}
	}

	// Insert it
	// Create cn splitted node
	sn = RTMALLOC(sizeof(node_t));
	if (sn == NULL) {
		return -1;
	}

	sn->bit = bit;
	sn->bit_len = (MEMBITS - bit);
	sn->key = (ip & MASK64(sn->bit, sn->bit_len));
	sn->parent = cn;
	sn->children[0] = NULL;
	sn->children[1] = NULL;
	sn->metadata = metadata;
	assert(sn->bit_len != 0);
	
	cn->children[child_idx] = sn;
	return 0;
}

int
rtree_delete(void *p)
{
}

void*
rtree_find(void *p)
{
	node_t *cn = &rtree_root;
	u64 ip = (u64)p;
	u8 bit = 0, child_idx;

	while (cn) {
		if ((ip & MASK64(cn->bit, cn->bit_len)) != cn->key)
			break;

		bit += cn->bit_len;
		if (cn->metadata) {
			assert(bit == MEMBITS);
			return cn->metadata;
		} else {
			child_idx = GETBIT(ip, bit);
			cn = cn->children[child_idx];
		}
	}

	return NULL;
}

void*
rtree_find_and_delete(void *p)
{
}


int
rtree_destroy()
{
}


