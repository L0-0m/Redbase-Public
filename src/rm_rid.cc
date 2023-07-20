//
// File:        rm_rid.cc
// Description: RID class implementation
// Authors:     L0-0m (rzwang@mail.ustc.edu.cn)
//

// We separate the interface of RID from the rest of RM because some
// components will require the use of RID but not the rest of RM.

#include "rm_rid.h"

//
// Defines
//
#define INVALID_PAGE   (-1)
#define INVALID_SLOT   (-1)

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RID::RID()
{
    pageNum = INVALID_PAGE;
    slotNum = INVALID_SLOT;
}   

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RID& RID::operator=(const RID &rid)
{
    if (this != &rid)
    {
        this->pageNum = rid.pageNum;
        this->slotNum = rid.slotNum;
    }

    return (*this);
}  

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RID::~RID()
{
    // Don't need to do anything.
}

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RID::RID(PageNum _pageNum, SlotNum _slotNum)
{
    this->pageNum = _pageNum;
    this->slotNum = _slotNum;
}

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RC RID::GetPageNum(PageNum &_pageNum) const
{
    if(pageNum < 0)
        return (GLOBAL_INVALIDRIDPAGE);
    
    _pageNum = this->pageNum;

    return (OK_RC);
}

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
bool RID::operator==(const RID &rid) const
{
    return ((this->pageNum == rid.pageNum)      &&
            (this->slotNum == rid.slotNum));
}  

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RC RID::GetSlotNum(SlotNum &_slotNum) const
{
    if(pageNum < 0)
        return (GLOBAL_INVALIDRIDSLOT);

    _slotNum = this->slotNum;

    return (OK_RC);
}

