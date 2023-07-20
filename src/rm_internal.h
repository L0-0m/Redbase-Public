//
// File:        rm_manager.cc
// Description: RM_Manager class implementation
// Authors:     L0-0m (rzwang@mail.ustc.edu.cn)
//

#ifndef RM_INTERNAL_H
#define RM_INTERNAL_H

#include <cstdlib>
#include <cstring>
#include "rm.h"

//
// PF_PageHdr: Header structure for pages
//
struct RM_PageHdr {
    PageNum nextFree;   // 下一个还有空余slot的pageNum
                        //  - PF_PAGE_LIST_END if this is last free page
                        //  - PF_PAGE_NOT_FREE if the page is full
    int recordNum;      // page中现有record个数
};

//
// Constants and defines
//
const int RM_PAGE_SIZE = PF_PAGE_SIZE - sizeof(RM_PageHdr);     // RM page的可用空间
const SlotNum RM_SLOT_EOF = -1;         // 为满足filescan逻辑功能，只能为-1

#define RM_PAGE_LIST_END  (-1)       // end of list of free pages
// #define RM_PAGE_NOT_FREE  (-2)       // full pages flag

#endif