//
// File:        rm_record.cc
// Description: RM_Record class implementation
// Authors:     Renzhong Wang (rzwang@mail.ustc.edu.cn)
//

#include "rm_internal.h"

//
// RM_Record
//
// Desc:
// Out:
// In:   
// Ret:  
//
RM_Record::RM_Record ()
{
    pData = NULL;
}

//
// ~RM_Record
//
// Desc:
// Out:
// In:   
// Ret:  
//
RM_Record::~RM_Record()
{
    if (pData != NULL)
        delete pData;
}

//
// operator=
//
// Desc:
// Out:
// In:   
// Ret:  
//
RM_Record& RM_Record::operator=(const RM_Record &rec)
{
    if(this != &rec)
    {
        // 拷贝RID
        this->rid = rec.rid;

        // 深拷贝
        this->recordSize = rec.recordSize;
        this->pData = new char[rec.recordSize];
        strncpy(this->pData, rec.pData, rec.recordSize);
    }

    return (*this);
}

//
// GetData
//
// Desc:
// Out:
// In:   
// Ret:  
//
RC RM_Record::GetData(char *&pData) const
{
    if(this->pData == NULL)
        return (RM_INVALIDRECORD);

    pData = this->pData;

    return (OK_RC);
}

//
// GetRid
//
// Desc:
// Out:
// In:   
// Ret:  
//
RC RM_Record::GetRid (RID &rid) const
{
    if(this->pData == NULL)
        return (RM_INVALIDRECORD);

    rid = this->rid;

    return (OK_RC);
}








