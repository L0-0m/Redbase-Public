//
// File:        ix_internal.h
// Description: Declarations internal to the indexing component
// Authors:     L0-0m (rzwang@mail.ustc.edu.cn)
//

#ifndef IX_INT_H
#define IX_INT_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "ix.h"

//
// IX_NodeHdr: Header structure for node
//
struct IX_NodeHdr {
    int level;			// 记录层次，用于区分当前节点类型
    int keyNum;			// 当前节点key-pointer对个数
	PageNum	extraPtr;	// 指向小于最小key的Node
	PageNum	prevPtr;	// 指向存储更小key的兄弟节点
};

//
// IX_BucketHdr: Header structure for bucket
//
struct IX_BucketHdr {
    int level;
	int ridNum;			// Bucket中rid个数
	PageNum	nextPtr;
	PageNum	prevPtr;
	int freeList;		// 以数组访问时，空闲位置的数组下标
	int useList;		// 以数组访问时，已使用位置的数组下标
};

//
// IX_RidEntry: Entry structure in bucket
//
struct IX_RidEntry {
	RID rid;
	short next;		// 以数组访问时，下一位置的数组下标
};

//
// Constants and defines
//
const int IX_LEAF_LEVEL   = 1;
const int IX_BUCKET_LEVEL = 0;
const int IX_NODE_SIZE = PF_PAGE_SIZE - sizeof(IX_NodeHdr);     // IX Node 的可用空间
const int IX_BUCKET_SIZE = PF_PAGE_SIZE - sizeof(IX_BucketHdr); // IX Bucket 的可用空间
const PageNum IX_INVALID_NODE = -1;								// B+树中的空Node
const int IX_RID_LIST_END = -1;								// bucket中rid链表的尾部

#endif