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

void* rtree_internal_find(rtree *root, void *p, int delete);
void rtree_internal_delete_node(rtree *root, node_t *node, int delete_metadata);
int rtree_internal_insert(rtree *root, void *p, void *metadata);

inline int
rtree_internal_insert(rtree *root, void *p, void *metadata)
{	
	node_t *cn = (node_t *)root->root, *sn;
	u64 ip = (u64)p, pp, x, k;
	u8 bit = 0, child_idx, l, yl, b, d;
	void *md;

	while (cn) {
		// Check partial address with stored key
		pp = ip & MASK64(bit, cn->bit_len);
		// XOR them
		x = pp ^ cn->key;
		// If they are the same, then insert to the child
		// Or if this is the last part of address, update the metadata?
		if (!x) {
			if (cn->bit_len + bit == MEMBITS) {
				if (root->rtree_cb_insert_duplication)
					(*root->rtree_cb_insert_duplication)(cn->metadata, metadata);
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

			if (!cn->children[child_idx]) {
				break;
			}

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
		if (cn->children[0]) 
			cn->children[0]->parent = sn;
		sn->children[1] = cn->children[1];
		if (cn->children[1]) 
			cn->children[1]->parent = sn;
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

	if (cn->children[0])
		rtree_internal_delete_node(root, cn->children[0], delete_metadata);
	if (cn->children[1])
		rtree_internal_delete_node(root, cn->children[1], delete_metadata);

	if (cn->metadata && delete_metadata && root->rtree_cb_delete) {
		(*root->rtree_cb_delete)(cn->metadata);
		cn->metadata = NULL;
	}

	if (parent) {
		if (parent->children[0] == cn) {
			parent->children[0] = NULL;
		} else if (parent->children[1] == cn) {
			parent->children[1] = NULL;
		} else {
			assert(0);
		}
	}

	RTFREE(cn);
}

inline void*
rtree_internal_find(rtree *root, void *p, int delete)
{
	node_t *cn = (node_t *)root->root;
	u64 ip = (u64)p;
	u8 bit = 0, child_idx;

	while (cn) {
		if ((ip & MASK64(cn->bit, cn->bit_len)) != cn->key)
			break;

		bit += cn->bit_len;
		if (cn->metadata) {
			assert(bit == MEMBITS);
			if (delete) {
				rtree_internal_delete_node(root, cn, 1);
				return NULL;
			} else 
				return cn->metadata;
		} else {
			child_idx = GETBIT(ip, bit);
			cn = cn->children[child_idx];
		}
	}

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
	rtree_internal_find(r, p, 1);
}

void*
rtree_find(rtree *r, void *p)
{
	return rtree_internal_find(r, p, 0);
}

int
rtree_init(rtree *r)
{
	node_t *root;

	if (!r) return -1;

	root = RTMALLOC(sizeof(node_t));
	root->key = 0;
	root->bit = 0;
	root->bit_len = MEMBITS;
	root->parent = NULL;
	root->children[0] = NULL;
	root->children[1] = NULL;
	root->metadata = NULL;

	r->root = (void *)root;
	r->rtree_cb_insert_duplication = NULL;
	r->rtree_cb_delete = NULL;
#ifdef RTREE_CUSTOM_ALLOCATION
	r->rtree_malloc = NULL;
	r->rtree_free = NULL;
#endif

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

