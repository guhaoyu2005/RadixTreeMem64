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

#include <stdint.h>
#include <assert.h>

#include "rtree.h"

#define u8 uint8_t
#define u64 uint64_t

struct rtree_node {
	// Address bits at current node
	u64 key;
	// Address bits length in key.
	// E.g. bit_len = 4, key(in hex) = 10 => [0010]
	u8 bit_len;
	u8 __pad[7];
	node_t *parent;
	// Binary tree: we are using addr in binary format so children starts
	// with either zero or one.
	node_t *children[2];
	// metadata (last node)
	void *metadata;
} node_t;

node_t rtree_root = {
    .key = 0,
    .bit_len = 0,
    .parent = NULL,
    .children[] = { NULL },
    .metadata = NULL
};

#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))

#define RTMALLOC malloc
#define RTFREE free

#define MEMBITS 64

int
rtree_insert(void *p, void *metadata)
{	
	node_t *cn = rtree_root, sn;
	u64 ip = (u64)p, pp, x, d, k;
	u8 bit = 0, child_idx, l;
	void *md;

	while (cn) {
		if (!cn->parent) {
			// Root stores nothing, go straight to the child
			goto insert;
		}

		// Check partial address with stored key
		pp = (ip >> (MEMBITS - cn->bit_len - bit)) & ((1 << cn->bit_len) - 1);
		// XOR them
		x = pp ^ cn->key;
		// If they are the same, then insert to the child
		// Or if this is the last part of address, update the metadata?
		if (!x) {
			if (cn->bit_len + bit == MEMBITS) {
				// TODO: update metadata???
				assert(0);
			}
			goto insert;
		}

		// Not same? Split child
		// which bit if different
		d = LOG2(x);
		child_idx = ((pp >> (d - 1)) &= 1);
		
		// save info for node splitted from cn  
		l = cn->bit_len - d;
		k = (cn->key & ((1 << l) - 1)); 
		md = cn->metadata;

		// update current node
		cn->key >>= d;
		cn->bit_len -= d;

		// Create cn splitted node
		sn = RTMALLOC(sizeof(node_t));
		sn->key = k;
		sn->bit_len = l;
		sn->parent = cn;
		sn->children[] = { NULL };
		sn->metadata = md;

		// Insert cn splitted node
		cn->children[child_idx ^ 1] = sn; 

insert:
		// update bit
		bit += cn->bit_len;

		child_idx = ((ip >> (MEMBITS - bit - 1)) &= 1);
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
	//TODO CHECK DO I NEED -1 here  dinner time no time to check
	sn->bit_len = (MEMBITS - bit - 1);
	sn->key = (ip & ((1 << sn->bit_len) - 1));
	sn->parent = cn;
	sn->children[] = { NULL };
	sn->metadata = metadata;
	
	cn->children[child_idx] = sn;
}

int
rtree_delete(void *p)
{
}

void*
rtree_find(void *p)
{
}

void*
rtree_find_and_delete(void *p)
{
}


int
rtree_destroy()
{
}


