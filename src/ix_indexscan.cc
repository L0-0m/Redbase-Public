//
// File:        ix_indexscan.cc
// Description: IX_IndexScan class implementation
// Authors:     Renzhong Wang (rzwang@mail.ustc.edu.cn)
//

#include "ix_internal.h"
#include "operations.h"

//
// IX_IndexScan
//
// Desc: 构造函数
//
IX_IndexScan::IX_IndexScan()
{
    bScanOpen = FALSE;
}

//
// ~IX_IndexScan
//
// Desc: 析构函数
//
IX_IndexScan::~IX_IndexScan()
{
    // nothing
}

//
// OpenScan
//
// Desc: 根据参数进行初始化，并返回第一个符合条件的rid位置信息，
//       为扫描一个已经打开的 RM_FileHandle 做好准备。
// In:   
// Out:
// Ret:
//
RC IX_IndexScan::OpenScan(const IX_IndexHandle  &_indexHandle,
                          CompOp                compOp,
                          void                  *_value,
                          ClientHint            _pinHint)
{
    if(bScanOpen == TRUE)
        return (IX_OPENEDSCAN);

	// 传入handle必须已经打开index
	if(_indexHandle.bIndexOpen == FALSE)
		return (IX_CLOSEDINDEX);

	pIxIh = (IX_IndexHandle*)&_indexHandle;   // 初始化FileHandle

	RC rc;
    PF_PageHandle ph;
    char *pData;

	// 比较方式检查
	if((compOp < NO_OP)    ||
	   (compOp > GE_OP)    ||
       (compOp == NE_OP))
	   return (RM_UNDEFCOMPOP);
    
    // Scan打开
    bScanOpen = TRUE;

    // 设定位置参数初值
    currentRidPos = IX_RID_LIST_END;
    bNext = TRUE;                   // 默认搜索方向向右
    
    // 若执行无添加查询...
    if(compOp == NO_OP)
    {
        _value = NULL;
        currentNode = pIxIh->hdr.leafList;
        currentEntryPos = sizeof(IX_NodeHdr);
        currentBucket = IX_INVALID_NODE;

        return (OK_RC);             // 函数返回
    }

    // 若执行条件查询...

    // 初始化比较函数
	switch (compOp)
	{
		case EQ_OP: Operate = Equal;                break;
		case LT_OP: Operate = LessThan;             break;
		case GT_OP: Operate = GreaterThan;          break;
		case LE_OP: Operate = LessThanOrEqual;      break;
		case GE_OP: Operate = GreaterThanOrEqual;   break;
		case NE_OP: Operate = NotEqual;             break;
	}

    // 复制value
    pValue = new char[pIxIh->hdr.attrLength];
    memcpy(pValue, _value, pIxIh->hdr.attrLength);

    // 找到value对应leaf节点
    currentNode = pIxIh->hdr.root;
    if(rc = FindLeaf(currentNode))
        return (rc);

    // 获取leaf上信息
    if((rc = pIxIh->pfFh.GetThisPage(currentNode, ph))  ||
       (rc = ph.GetData(pData)))
        return (rc);

    // 找到leaf上entry（返回值满足GE情况下需求）
    int entryLength = pIxIh->hdr.attrLength + 4;
    if(rc = pIxIh->BinarySearch(pValue, currentNode, currentEntryPos, currentBucket))
        return (rc);

    // 根据情况调整currentNode，并获取currentEntryPos
    // TODO 有一定冗余计算，可删减
    PageNum tempNode = currentNode;
    if(compOp == EQ_OP)
    {   
        if(currentBucket == IX_INVALID_NODE)
            currentNode = IX_INVALID_NODE;      // 未找到
    }
    else if(compOp == LT_OP)
    {
        bNext = FALSE;

        // 无论是否找到，pos向左移动一个entry
        if(currentEntryPos == sizeof(IX_NodeHdr))   // 冗余
            currentNode = ((IX_NodeHdr*)pData)->prevPtr;
        else
            currentEntryPos -= entryLength;
    }
    else if(compOp == GT_OP)
    {
        // 如果找到了，则向右移动一个entry
        if(currentBucket != IX_INVALID_NODE)
        {
            // 冗余
            if(currentEntryPos == sizeof(IX_NodeHdr) + entryLength * ((IX_NodeHdr*)pData)->keyNum)
                currentNode = ((IX_NodeHdr*)pData)->extraPtr;
            else
                currentEntryPos += entryLength;
        }
    }
    else if(compOp == LE_OP)
    {
        bNext = FALSE;
        
        // 未找到情况下进pos向左移动一个entry
        if(currentBucket == IX_INVALID_NODE)
        {
            if(currentEntryPos == sizeof(IX_NodeHdr))
                currentNode = ((IX_NodeHdr*)pData)->prevPtr;    // 冗余
            else
                currentEntryPos -= entryLength;
        }
    }

    // unpin
    if(rc = pIxIh->pfFh.UnpinPage(tempNode))
        return (rc);

    return (OK_RC);
}

//
// GetNextEntry
//
// Desc: 根据位置参数返回rid，然后向后更新一次位置参数，为下一次读取做准备。
// In:   rid - 新建的rid
// Out:  rid - 找到的rid
//
RC IX_IndexScan::GetNextEntry(RID &rid)
{
    RC rc;
    PF_PageHandle ph;
    char *pBucketData;

    // 判断位置参数合理性
    if(currentNode != IX_INVALID_NODE)
    {   // 参数有效，获取返回值...

        // 获取Bucket上信息
        if((rc = pIxIh->pfFh.GetThisPage(currentBucket, ph))  ||
           (rc = ph.GetData(pBucketData)))
            return (rc);

        if(currentRidPos == IX_RID_LIST_END)
            currentRidPos = ((IX_BucketHdr*)pBucketData)->useList;

        rid = ((IX_RidEntry*)(pBucketData + currentRidPos))->rid;

        if(rc = pIxIh->pfFh.UnpinPage(currentBucket))     // unpin
            return (rc);    

        // 更新位置参数
        if(rc = GetNextPos())
            return (rc);
        
        return (OK_RC);
    }
    else
    {   // 参数无效，返回EOF...
        
        return (IX_EOF);
    }
}

//
// GetNextPos
//
// Desc: 根据位置参数返回下一个rid的位置参数。
//
RC IX_IndexScan::GetNextPos()
{
    RC rc;
    PF_PageHandle ph;
    char *pNodeData, *pBucketData, *pNewNodeData;
    int entryLength = pIxIh->hdr.attrLength + 4;
    int hdrSize = sizeof(IX_NodeHdr);
    PageNum tempNode;
    int keyNum;
    
    PageNum newNode;

    do 
    {
            //////////////////////////////////////////////////////////////////
            //                    遍历当前entry指向的bucket                    //
            //////////////////////////////////////////////////////////////////

            do
            {   // 获取Bucket上信息
                tempNode = currentBucket;
                if((rc = pIxIh->pfFh.GetThisPage(tempNode, ph))  ||
                (rc = ph.GetData(pBucketData)))
                    return (rc);

                // 更新ridPos
                if(currentRidPos == IX_RID_LIST_END)
                    currentRidPos = ((IX_BucketHdr*)pBucketData)->useList;
                else
                    currentRidPos = ((IX_RidEntry*)(pBucketData + currentRidPos))->next;

                if(currentRidPos != IX_RID_LIST_END)
                {
                    // unpin currentBucket
                    if(rc = pIxIh->pfFh.UnpinPage(tempNode))
                        return (rc);
                    
                    return (OK_RC);
                }

                currentBucket = ((IX_BucketHdr*)pBucketData)->nextPtr;
            
                // unpin currentBucket
                if(rc = pIxIh->pfFh.UnpinPage(tempNode))
                    return (rc);

            }while (currentBucket != IX_INVALID_NODE);
                                

            //////////////////////////////////////////////////////////////////
            //                            更新entry                         //
            //////////////////////////////////////////////////////////////////

            if(bNext)
                currentEntryPos += entryLength;
            else
                currentEntryPos -= entryLength;


            // 获得当前node上keyNum
            if((rc = pIxIh->pfFh.GetThisPage(currentNode, ph))  ||
               (rc = ph.GetData(pNodeData)))                                    return (rc);
                
            keyNum = ((IX_NodeHdr*)pNodeData)->keyNum;

            if(rc = pIxIh->pfFh.UnpinPage(currentNode))                         return (rc);
                

            // 若entryPos不合理，找新node上entry，并更新node
            if((currentEntryPos < hdrSize)  ||  (currentEntryPos == hdrSize + entryLength * keyNum))
            {   
                // 获得newNode
                if((rc = pIxIh->pfFh.GetThisPage(currentNode, ph))  ||
                   (rc = ph.GetData(pNodeData)))                                return (rc);

                if(bNext)
                {   // 获得右边node
                    newNode = ((IX_NodeHdr*)pNodeData)->extraPtr;
                    if(newNode != IX_INVALID_NODE)
                        // 从第一个entry开始
                        currentEntryPos = hdrSize;
                }
                else
                {   // 获得左边node
                    newNode = ((IX_NodeHdr*)pNodeData)->prevPtr;
                    if(newNode != IX_INVALID_NODE)
                    {
                        if((rc = pIxIh->pfFh.GetThisPage(newNode, ph))  ||
                           (rc = ph.GetData(pNewNodeData)))             return (rc);

                        // 从最后一个entry开始
                        currentEntryPos = hdrSize + (entryLength * (((IX_NodeHdr*)pNewNodeData)->keyNum - 1));

                        if(rc = pIxIh->pfFh.UnpinPage(newNode))         return (rc);
                    }
                }

                if(rc = pIxIh->pfFh.UnpinPage(currentNode))                     return (rc);

                // 更新node
                if(newNode != IX_INVALID_NODE)
                    currentNode = newNode;
                else
                    break;      // 跳出
            }

            //////////////////////////////////////////////////////////////////
            //                       判断新entry是否符合条件                   //
            //////////////////////////////////////////////////////////////////

            if((rc = pIxIh->pfFh.GetThisPage(currentNode, ph))  ||
               (rc = ph.GetData(pNodeData)))                                    return (rc);

            if((pValue != NULL)     &&
               (FALSE == Operate(pNodeData + currentEntryPos, pValue, pIxIh->hdr.attrType, pIxIh->hdr.attrLength)))
            {   // 不符合条件...

                if(rc = pIxIh->pfFh.UnpinPage(currentNode))                     return (rc);
                break;   // 跳出
            }
            else
            {
                // 获得新的bucket
                currentBucket = *(PageNum*)(pNodeData + currentEntryPos + pIxIh->hdr.attrLength);

                if(rc = pIxIh->pfFh.UnpinPage(currentNode))                     return (rc);
            }

        }while(1);

    currentNode = IX_INVALID_NODE;

    return (OK_RC);
}

//
// CloseScan
//
// Desc: 关闭scan
//
RC IX_IndexScan::CloseScan()
{
	// 不能关闭一个已经关闭的scan
	if(bScanOpen == FALSE)
		return (IX_CLOSEDSCAN);

    if(pValue != NULL)
        delete []pValue;

	pIxIh = NULL;

	// 关闭 scan
	bScanOpen = FALSE;

	return (OK_RC);
}

//
// FindLeaf
//
// Desc: 从root向下寻找value所在的leafNode
// In:   thisNode - 根节点
// Out:  thisNode - value所在leafNode
// Ret:  IX return code
//
RC IX_IndexScan::FindLeaf(PageNum &thisNode)
{
    RC rc;
    int pos;
    PageNum nextNode;

    for(int i = pIxIh->hdr.height; i > IX_LEAF_LEVEL; --i)
    {
        if(rc = pIxIh->BinarySearch(pValue, thisNode, pos, nextNode))
            return (rc);
    
        thisNode = nextNode;
    }

    return (OK_RC);
}