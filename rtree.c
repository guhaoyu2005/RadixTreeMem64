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
#else
#define RTMALLOC (root->rtree_malloc)
#define RTFREE (root->rtree_free)
#endif

#define u8 uint8_t
#define u64 uint64_t

#define MEMBITS 64
#define CHUNK 1  //TODO: support chunk 2, 4...

#ifdef RTREE_DEBUG
void
PB(u64 n) 
{
	for (int i=MEMBITS-1; i>=0; i--) {
		printf("%d", (((((u64)1)<<(u64)i) & n) > 0));
	}
	printf("\n");
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
	node_t *children[1<<CHUNK];
	// metadata (last node)
	void *metadata;
};

#define LOG2(X) ((unsigned) (8*sizeof (unsigned long long) - __builtin_clzll((X)) - 1))
#define MASK64(B, L) ( (B) == 0 ? \
    ( ((u64)(-1)) - ((L)>0 ? (((u64)1) << (MEMBITS - (B) - (L))) - 1 : 0) ) : \
    ( L>0 ? \
      ((((u64)1) << (MEMBITS - (B))) - 1 - ((((u64)1) << (MEMBITS - (B) - (L))) - 1)) : \
      ((u64)-1) ))
#define GETBIT(X, N) (((X) & (((u64)1) << (MEMBITS - (N) - 1))) > 0)
#define GETKEY(X) ((X->key) & MASK64(X->bit, X->bit_len))

void* rtree_internal_find(rtree *root, void *p, int del, int le);
void rtree_internal_delete_node(rtree *root, node_t *node, int delete_metadata);
int rtree_internal_insert(rtree *root, void *p, void *metadata);
node_t* rtree_internal_find_node_largest(node_t *cn, node_t *source, void *p, 
    u8 bit);

inline int
rtree_internal_insert(rtree *root, void *p, void *metadata)
{	
	node_t *cn = (node_t *)root->root, *sn;
	u64 ip = (u64)p, pp, x, k;
	u8 bit = 0, child_idx, l, yl, b, d;
	void *md;

	while (cn) {
		assert(bit == cn->bit);
		// Check partial address with stored key
		pp = ip & MASK64(bit, cn->bit_len);
		// XOR them
		x = pp ^ GETKEY(cn);
		// If they are the same, then insert to the child
		// Or if this is the last part of address, update the metadata?
		if (!x) {
			if (cn->bit_len + bit == MEMBITS) {
				if (root->rtree_cb_insert_duplication)
					(*root->rtree_cb_insert_duplication)(
					    cn->metadata, metadata
					);
				return 0;
			}
			
			bit += cn->bit_len;
			child_idx = GETBIT(ip, bit);

			if (!cn->children[child_idx]) {
				break;
			}

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

			assert(bit <= MEMBITS);

			if (!cn->children[child_idx]) {
				break;
			}

			cn = cn->children[child_idx];
			continue;
		}
		
		// save info for node splitted from cn  
		b = MEMBITS - d - 1;
		l = d + 1;
		k = cn->key; 
		md = cn->metadata;

		// update current node
		cn->bit_len -= yl;
		cn->key &= MASK64(0, cn->bit + cn->bit_len);

		assert((cn->key & MASK64(0, cn->bit + cn->bit_len)) == 
		    (ip & MASK64(0, cn->bit + cn->bit_len)));
		assert(cn->bit_len <= MEMBITS);

		// Create cn splitted node
		sn = (node_t *)RTMALLOC(sizeof(node_t));
		if (sn == NULL) {
			return -1;
		}
		sn->key = k;
		sn->bit = b;
		sn->bit_len = yl;
		sn->parent = cn;
		sn->children[0] = cn->children[0];
		if (cn->children[0]) 
			cn->children[0]->parent = sn;
		sn->children[1] = cn->children[1];
		if (cn->children[1]) 
			cn->children[1]->parent = sn;
		sn->metadata = md;

		assert(l != 0);
		assert(sn->bit_len <= MEMBITS);

		// Insert cn splitted node
		cn->children[child_idx ^ 1] = sn; 
		cn->children[child_idx] = NULL;
		cn->metadata = NULL;

		// update bit
		bit += cn->bit_len;
		
		assert(bit <= MEMBITS);

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
	sn = (node_t *)RTMALLOC(sizeof(node_t));
	if (sn == NULL) {
		return -1;
	}

	sn->bit = bit;
	sn->bit_len = (MEMBITS - bit);
	sn->key = (ip & MASK64(0, sn->bit + sn->bit_len));
	sn->parent = cn;
	sn->children[0] = NULL;
	sn->children[1] = NULL;
	sn->metadata = metadata;

	assert(sn->bit_len != 0);
	assert(sn->bit + sn->bit_len == MEMBITS);
	assert(sn->key == ip);
	
	cn->children[child_idx] = sn;
	return 0;
}

/*
 * Only supports fast delete now -> if the node on path is single child, we
 * don't compress it. 
 * TODO: add compact delete so saves space.
 */
void
rtree_internal_delete_node(rtree *root, node_t *node, int delete_metadata)
{
	node_t *cn = node, *parent;

	parent = cn->parent;

	for (int i=0;i<(1<<CHUNK);i++) {
		if (!cn->children[i])
			continue;
		rtree_internal_delete_node(root, cn->children[i], delete_metadata);
	}

	if (cn->metadata && delete_metadata) {
		if (root->rtree_cb_delete)
			(*root->rtree_cb_delete)(cn->metadata);
		cn->metadata = NULL;
	}

	if (parent) {
		for (int i=0;i<(1<<CHUNK);i++) {
			if (parent->children[i] == cn) {
				parent->children[i] = NULL;
			}
		}
	}

	RTFREE(cn);
}

inline void*
rtree_internal_find(rtree *root, void *p, int del, int le)
{
	node_t *cn = (node_t *)root->root;
	u64 ip = (u64)p;
	u8 bit = 0, child_idx;

	assert(~(del & le));

	while (cn) {
		if ((ip & MASK64(cn->bit, cn->bit_len)) != GETKEY(cn)) {
			if (le) {
				cn = cn->parent;
				if (!cn) {
					return NULL;
				} else {
					bit -= cn->bit_len;
				}
			}
			break;
		}

		bit += cn->bit_len;
		if (cn->metadata) {
			assert(bit == MEMBITS);
			if (del) {
				rtree_internal_delete_node(root, cn, 1);
				return NULL;
			} else 
				return cn->metadata;
		} else {
			child_idx = GETBIT(ip, bit);
			if (le && cn->children[child_idx] == NULL) {
				bit -= cn->bit_len;
				break;
			}
			cn = cn->children[child_idx];
		}
	}

	// Now we are at the last node that the key is equivalent 
	// if less-equal toggle is on, we grab the right-most node
	if (le) {
		assert(cn);
		assert(bit < MEMBITS);

		cn = rtree_internal_find_node_largest(cn, NULL, p, bit);
		if (cn) {
			return cn->metadata;
		}
	}

	return NULL;
}

node_t* 
rtree_internal_find_node_largest(node_t *cn, node_t *source, void *p, u8 bit)
{
	node_t *n, *rt;
	u64 addr;

	if (!cn) return NULL;

	assert(bit == cn->bit);
	
	// Check self first
	addr = cn->key;

	// If current node, whose addr. is smaller than target, 
	// contains metadata then return
	if (addr < (u64)p) {
		// Total checking order guarantees the first seen node is target
		if (cn->metadata) {
			assert(bit + cn->bit_len == MEMBITS);
			return cn;
		}
	} else {
		assert(!(((cn->bit + cn->bit_len) == MEMBITS) && (addr == (u64)p)));
	}

	// Always do recursive check from highest children to lowest
	for (int i=(1<<CHUNK)-1;i>=0;i--) {
		n = cn->children[i];
		if (n == source)
			continue;
		
		rt = rtree_internal_find_node_largest(n, cn, p, bit+cn->bit_len);
		if (rt) 
			return rt;
	}

	// Lastly check parent
	if (source != cn->parent && cn->parent != NULL)
		return rtree_internal_find_node_largest(cn->parent, cn, p, 
		    bit - cn->parent->bit_len);
	else
		return NULL;
}

/*
 *================================================================
 * Interfaces
 *================================================================
 */
int
rtree_insert(rtree *r, void *p, void *metadata)
{
	return rtree_internal_insert(r, p, metadata);
}

void
rtree_delete(rtree *r, void *p)
{
	rtree_internal_find(r, p, 1, 0);
}

void*
rtree_find(rtree *r, void *p)
{
	return rtree_internal_find(r, p, 0, 0);
}

void*
rtree_find_le(rtree *r, void *p)
{
	return rtree_internal_find(r, p, 0, 1);
}

int
rtree_init(rtree *root)
{
	node_t *r;

	if (!root) return -1;

	r = (node_t *)RTMALLOC(sizeof(node_t));
	r->key = 0;
	r->bit = 0;
	r->bit_len = MEMBITS;
	r->parent = NULL;
	r->children[0] = NULL;
	r->children[1] = NULL;
	r->metadata = NULL;

	root->root = (void *)r;
	return 0;
}

int
rtree_destroy(rtree *root)
{
	node_t *root_node;

	if (!root) return -1;

	root_node = (node_t *)root->root;
	rtree_delete(root, NULL);
	RTFREE(root_node);

	return 0;
}

