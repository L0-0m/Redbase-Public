//
// rm_rid.h
//
//   The Record Id interface
//

#ifndef RM_RID_H
#define RM_RID_H

// We separate the interface of RID from the rest of RM because some
// components will require the use of RID but not the rest of RM.

#include "redbase.h"

//
// PageNum: uniquely identifies a page in a file
//
typedef int PageNum;

//
// SlotNum: uniquely identifies a record in a page
//
typedef int SlotNum;

//
// RID: Record id interface
//
class RID {
public:
    RID();                                         // Default constructor
    RID(PageNum pageNum, SlotNum slotNum);
    RID& operator=(const RID &rid);
    ~RID();                                        // Destructor

    RC GetPageNum(PageNum &pageNum) const;         // Return page number
    RC GetSlotNum(SlotNum &slotNum) const;         // Return slot number
    bool operator==(const RID &rid) const;

private:
    PageNum pageNum;        // pageNum从1开始
    SlotNum slotNum;        // slotNum从0开始
};

// Warnings
#define GLOBAL_INVALIDRIDPAGE   (START_GLOBAL_WARN + 0) // Invalid page # in RID
#define GLOBAL_INVALIDRIDSLOT   (START_GLOBAL_WARN + 1) // Invalid slot # in RID

#endif
