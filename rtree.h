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
#ifndef __RTREE_H_
#define __RTREE_H_

typedef struct rtree_instance {
	void* root;
	// callbacks
	//  rtree_cb_insert_duplication(current, new)
	void (*rtree_cb_insert_duplication)(void *, void *);
	void (*rtree_cb_delete)(void *);
#ifdef RTREE_CUSTOM_ALLOCATION
	void* (*rtree_malloc)(int size);
	void (*rtree_free)(void *ptr);
#endif
} rtree;

int rtree_insert(rtree *r, void *p, void *metadata);
void rtree_delete(rtree *r, void *p);

void* rtree_find(rtree *r, void *p);
// Find address exactly the same or the closest address this is smaller
void* rtree_find_le(rtree *r, void *p);

int rtree_init(rtree *r);
int rtree_destroy(rtree *r);

#endif
