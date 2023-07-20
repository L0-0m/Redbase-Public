//
// File:        ix_indexhandle.cc
// Description: IX_IndexHandle class implementation
// Authors:     L0-0m (rzwang@mail.ustc.edu.cn)
//

#include "ix_internal.h"
#include "operations.h"

//
// IX_IndexHandle
//
// Desc: 构造函数。初始化将索引描述符设为关闭。
//
IX_IndexHandle::IX_IndexHandle()
{
    bIndexOpen = FALSE;
}

//
// ~IX_IndexHandle
//
// Desc: 析构函数
//
IX_IndexHandle::~IX_IndexHandle()
{
    // Don't need to do anything
}

//
// InsertEntry
//
// Desc: 执行插入函数，根据key插入rid。插入结束后，根据key指针判断是否需增殖新的root。
// In:   pKey - 指向要插入索引中的属性值
//       rid  - 标识该属性值所在record的标识符
// Ret:  IX return code
//
RC IX_IndexHandle::InsertEntry(void *key, const RID &rid)
{   
    RC rc;
    PageNum childNode = hdr.root;
    PF_PageHandle ph;
    char *pData, *pTemp = new char[hdr.attrLength];
    void *pKey = pTemp;
    int pos;

    // 复制key，防止改动原本值
    memcpy(pKey, key, hdr.attrLength);

    // 递归插入(key, rid)
    if(rc = InsertKey(pKey, childNode, rid))
        return (rc);

    // 若需要分裂新的root
    if (pKey != NULL)
    {
        PageNum oldRoot = hdr.root;

        // 创建一个leaf作为新root
        if(rc = CreateNode(hdr.root, hdr.height + 1))
            return (rc);

        // 获得内容指针
        if((rc = pfFh.GetThisPage(hdr.root, ph))    ||
           (rc = ph.GetData(pData)))
           return (rc);

        // 设置extra指针
        ((IX_NodeHdr*)pData)->extraPtr = oldRoot;

        // 若为leaf，先插入rid
        if(childNode == IX_INVALID_NODE)
        {
            if((rc = CreateBucket(childNode))   ||
               (rc = InsertBucket(childNode, rid)))
                return (rc);
        }

        // 插入(key, ptr)
        memcpy(pData + sizeof(IX_NodeHdr), (char*)pKey, hdr.attrLength);
        *(PageNum*)(pData + sizeof(IX_NodeHdr) + hdr.attrLength) = childNode;
        ((IX_NodeHdr*)pData)->keyNum++;

        if((rc = pfFh.MarkDirty(hdr.root)   ||
           (rc = pfFh.UnpinPage(hdr.root))))
            return (rc);
    }

    delete []pTemp;

    return (OK_RC);
}

//
// DeleteEntry
//
// Desc: 根据pKey删除rid
// In:   pKey - 指向要删除索引中的属性值
//       rid  - 标识该属性值所在record的标识符
// Ret:  IX return code
//
RC IX_IndexHandle::DeleteEntry(void *pKey, const RID &rid)
{
    RC rc;
    PageNum root = hdr.root;
    PageNum done = IX_INVALID_NODE;

    balanceNode = IX_INVALID_NODE;
    pLeftAnchorKey = NULL;

    // 拷贝pKey，防止修改到原来的值
    char *pOrigin = new char[hdr.attrLength];
    char *pTemp = pOrigin;
    memcpy(pTemp, pKey, hdr.attrLength);

    if(rc = FindRebalance(done, root, IX_INVALID_NODE, IX_INVALID_NODE, 
                                      IX_INVALID_NODE, IX_INVALID_NODE, pTemp, rid))
        return (rc);

    delete []pOrigin;

    return (OK_RC);
}

//
// ForcePages
//
// Desc: 将*所有*与 Index 相关page写回磁盘。若IX_IndexHdr改动，则同样要写回。
// Ret:  IX return code
//
RC IX_IndexHandle::ForcePages()
{
    RC rc;
    PF_PageHandle ph;
    char *pData;
    PageNum hdrPageNum;

    // Index must be open
    if (!bIndexOpen)
      return (IX_CLOSEDINDEX);

    // 如果索引文件头被改动，将其写回文件
    if(bHdrChanged)
    {
        // 读入hdr对应page
        if((rc = pfFh.GetFirstPage(ph))         ||
           (rc = ph.GetPageNum(hdrPageNum)))
            return (rc);

        // 检查存储hdr的page合法性
        if(hdrPageNum)
            return (IX_ISNOTHDRPAGE);

        // 更新hdr信息
        if(rc = ph.GetData(pData))
            return (rc);

        *(IX_IndexHdr*)pData = this->hdr;

        // set dirty, unpinned
        if((rc = pfFh.MarkDirty(hdrPageNum))    ||
           (rc = pfFh.UnpinPage(hdrPageNum)))
            return (rc);

        // This function is declared const, but we need to change the
        // bHdrChanged variable.  Cast away the constness
        IX_IndexHandle *dummy = (IX_IndexHandle *)this;
        dummy->bHdrChanged = FALSE;
    }

    // 调用PF层函数写回指定page（参数省略即为 ALL PAGES）
    return (pfFh.ForcePages());
}    

//
// InsertKey
//
// Desc: 从root递归下降到leaf层次后执行插入，解递归过程中执行可能的分裂与插入。
// In:   pKey     - 指向待插入的key的指针
//       thisNode - 当前所处理node的pageNum
//       rid      - key对应的记录地址
// Out:  pKey     - 成功插入key并无需增殖root则置空，否则不变。
//       thisNode - 当pKey非空时，表示与thisNode新分裂出的邻居节点
// Ret:  IX return code
//
RC IX_IndexHandle::InsertKey(void *&pKey, PageNum &thisNode, const RID &rid)
{
    // B+树为空
    if (thisNode == IX_INVALID_NODE)
        return (OK_RC);

    RC rc;
    PageNum childNode, tempNode = IX_INVALID_NODE;
    char *pData;
    PF_PageHandle ph;
    int pos;

    // 查找thisNode上key对应childNode，返回(pos, childNode)
    rc = BinarySearch(pKey, thisNode, pos, childNode);

    // 获得thisNode数据指针
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
       (rc = ph.GetData(pData)))
        return (rc);

    // 递归向下
    if(((IX_NodeHdr*)pData)->level != IX_LEAF_LEVEL)
    {
        if(rc = InsertKey(pKey, childNode, rid))
            return (rc);
    }   

    // 无需插入，直接返回
    if(pKey == NULL)
    {
        // unpin
        if(rc = pfFh.UnpinPage(thisNode))
            return (rc);
        
        thisNode = IX_INVALID_NODE;

        return (OK_RC);
    }
    
    // thisNode已满
    if(((IX_NodeHdr*)pData)->keyNum == hdr.keyNumPerPage)
    {
        // 若为leaf，则需要创建Bucket
        if(((IX_NodeHdr*)pData)->level == IX_LEAF_LEVEL)
        {
            if((rc = CreateBucket(childNode))   ||
               (rc = InsertBucket(childNode, rid)))
                return (rc);        
        }

        // 在thisNode的pos处中插入(pKey, childNode)
        // 分裂后返回新的(pKey, childNode)
        if(rc = SplitNode(pKey, childNode, thisNode, pos))
            return (rc);

        tempNode = childNode;   // 返回值
    }
    else
    {   // 在thisNode的pos处插入(pKey, childNode, rid)
        
        // 若为leaf，且leaf中已存在entry，直接插入其Bucket中
        if((((IX_NodeHdr*)pData)->level == IX_LEAF_LEVEL)   &&
           (childNode != IX_INVALID_NODE))
        {
            if(rc = InsertBucket(childNode, rid))
                    return (rc);
        }
        else
        {   // 若为leaf且其中无对应entry，则先创建Bucket并插入rid
            if(childNode == IX_INVALID_NODE)
            {
                if((rc = CreateBucket(childNode))   ||
                   (rc = InsertBucket(childNode, rid)))
                    return (rc);
            }

            // 插入node
            if(rc = InsertNode(pKey, thisNode, childNode, pos))
                return (rc);
        }

        // 设置返回值
        pKey = NULL;
    }

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(thisNode))  ||
       (rc = pfFh.UnpinPage(thisNode)))
        return (rc);    

    if(tempNode != IX_INVALID_NODE)
        thisNode = tempNode;

    return (OK_RC);
}

//
// CreateNode
//
// Desc: 生成新的Node（可用作root、internal、leaf），写入文件头，并进行指针初始化。
// In:   level   - 节点层次
// Out:  newNode - 新node的pageNum
// Ret:  IX return code
//
RC IX_IndexHandle::CreateNode(PageNum &newNode, int level)
{
    RC rc;
    PF_PageHandle ph;
    char* pData;
    int offset;

    if(level > hdr.height + 1)
        return (IX_INVALIDNODEHEIGHT);

    // 分配新page
    if((rc = pfFh.AllocatePage(ph)) ||
       (rc = ph.GetData(pData))     ||
       (rc = ph.GetPageNum(newNode)))
        return (rc);

    // 若为root则更新树高
    if(level == hdr.height + 1)
    {   
        if(level == IX_LEAF_LEVEL)
            hdr.leafList = newNode;
        
        hdr.height++;
        bHdrChanged = TRUE;
    }

    // 写入Hdr信息
    *(IX_NodeHdr*)pData = {level,           // level
                           0,               // keyNum
                           IX_INVALID_NODE, // extraPtr
                           IX_INVALID_NODE};// prevPtr

    // key-pointer对数组起始位置
    offset = sizeof(IX_NodeHdr);

    // 初始化节点内pointer对（将指针置空）
    for(int i = 0; i < hdr.keyNumPerPage; ++i)
    {
        *(PageNum*)(pData + offset + hdr.attrLength) = IX_INVALID_NODE;
        offset += (4 + hdr.attrLength);
    }

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(newNode))   ||
       (rc = pfFh.UnpinPage(newNode)))
       return (rc);

    return (OK_RC);
}

//
// CreateBucket
//
// Desc: 生成新的Bucket，写入文件头，并进行指针初始化。
// Out:  newNode - 新bucket的pageNum
// Ret:  IX return code
//
RC IX_IndexHandle::CreateBucket(PageNum &newNode)
{
    RC rc;
    PF_PageHandle ph;
    char* pData;
    int i;

    // 分配新page
    if((rc = pfFh.AllocatePage(ph)) ||
       (rc = ph.GetData(pData))     ||
       (rc = ph.GetPageNum(newNode)))
        return (rc);

    int ridEntrySize = sizeof(IX_RidEntry);
    int offset = sizeof(IX_BucketHdr);

    // 写入Hdr信息
    *(IX_BucketHdr*)pData = {IX_BUCKET_LEVEL,   // level 
                             0,                 // ridNum
                             IX_INVALID_NODE,   // extraPtr
                             IX_INVALID_NODE,   // prevPtr
                             offset,            // freeList
                             IX_RID_LIST_END }; // useList

    // 将Bucket中空闲位置组织成freeList
    for(i = 0; i < hdr.ridNumPerPage - 1; ++i)
    {  
        ((IX_RidEntry*)(pData + offset))->next = offset + ridEntrySize;
        offset += ridEntrySize;
    }
    ((IX_RidEntry*)(pData + offset))->next = IX_RID_LIST_END;

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(newNode))   ||
       (rc = pfFh.UnpinPage(newNode)))
       return (rc);

    return (OK_RC);
}

//
// SplitNode
//
// Desc: thisNode已满情况下，扩充thisNode空间（通过开辟1个entry大小的临时空间）。
//       然后在pos处插入(pKey, childNode)，将thisNode上一半entries转移到新建
//       的newNode中，并得到父指针，将二者以(pKey, childNode)形式返回。
// In :  pKey      - 待插入key
//       childNode - 带插入key对应的childNode
//       thisNode  - 当前要插入的Node
//       pos       - 待插入key位置的偏置
// Out:  pKey      - 分裂后产生的父节点key
//       childNode - 新Node的pageNum
// Ret:  IX return code
//
RC IX_IndexHandle::SplitNode(void *&pKey, PageNum &childNode, PageNum thisNode, int pos)
{
    RC rc;
    PF_PageHandle ph;
    char *pThisData, *pNewData;

    // 读取当前node
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
       (rc = ph.GetData(pThisData)))
       return (rc);

    int level  = ((IX_NodeHdr*)pThisData)->level;
    int keyNum = ((IX_NodeHdr*)pThisData)->keyNum;
    int entryLength = hdr.attrLength + 4;

    // 判断是否需要分裂
    if(keyNum < hdr.keyNumPerPage)
    {
        // unpin
        pfFh.UnpinPage(thisNode);
        return (IX_DONTNEEDSPLIT);
    }

    PageNum newNode, tempChildNode;
    char *pTempKey = new char[hdr.attrLength];  // 临时空间

    // 将多出来的entry放入临时空间中，并插入pos
    int offset = sizeof(IX_NodeHdr) + entryLength * keyNum;
    if(pos == offset)
    {
        // 待插入entry即为最后一个，直接移入临时空间
        memcpy(pTempKey, pKey, hdr.attrLength);
        tempChildNode = childNode;
    }
    else
    {   // 否则，将thisNode中最后一个entry放入临时空间
        offset -= entryLength;
        memcpy(pTempKey, pThisData + offset, hdr.attrLength);
        tempChildNode = *(PageNum*)(pThisData + offset + hdr.attrLength);
        ((IX_NodeHdr*)pThisData)->keyNum--;         // 更新keyNum

        // 插入
        // TODO 考虑更高效实现，如根据pos位置先将一半entries移动到newNode
        InsertNode(pKey, thisNode, childNode, pos);
        
    }   // assert(((IX_NodeHdr*)pThisData)->keyNum == hdr.keyNumPerPage)

    // 新建newNode
    if((rc = CreateNode(newNode, level))    ||
       (rc = pfFh.GetThisPage(newNode, ph)) ||
       (rc = ph.GetData(pNewData)))
        return (rc);

    // 计算thisNode、newNode分裂后keyNum
    int restKeyNumInThis = (hdr.keyNumPerPage + 1) / 2;
    int restKeyNumInNew  = hdr.keyNumPerPage + 1 - restKeyNumInThis;

    // 生成待返回的entry
    int offsetThis = sizeof(IX_NodeHdr) + entryLength * restKeyNumInThis;
    int offsetNew  = sizeof(IX_NodeHdr);
    memcpy(pKey, pThisData + offsetThis, hdr.attrLength);
    childNode = newNode;

    // 若为内部节点分裂，则newNode不包含待插入父节点的entry
    if(level != IX_LEAF_LEVEL)
    {   
        // 生成extra指针
        ((IX_NodeHdr*)pNewData)->extraPtr = *(PageNum*)(pThisData + offsetThis + hdr.attrLength);
        
        // 将该entry从thisNode中删掉
        offsetThis += entryLength;
        restKeyNumInNew--;
    }

    // 将thisNode中一半entries移入newNode
    int temp = restKeyNumInNew - 1;
    memcpy(pNewData  + offsetNew, 
           pThisData + offsetThis,
           temp      * entryLength);
    offsetNew += (temp * entryLength);

    // 将临时空间中entry移入newNode
    memcpy(pNewData + offsetNew, pTempKey, hdr.attrLength);
    memcpy(pNewData + offsetNew + hdr.attrLength, &tempChildNode, 4);
    delete []pTempKey;  // 释放临时空间

    // 更新thisNode、newNode hdr
    ((IX_NodeHdr*)pThisData)->keyNum = restKeyNumInThis;
    ((IX_NodeHdr*)pNewData)->keyNum = restKeyNumInNew;

    // 若为leaf，更新双向指针
    if(level == IX_LEAF_LEVEL)
    {
        char *pTemp;
        PageNum tempNode = ((IX_NodeHdr*)pThisData)->extraPtr;
        ((IX_NodeHdr*)pNewData)->extraPtr = tempNode;
        ((IX_NodeHdr*)pNewData)->prevPtr = thisNode;
        ((IX_NodeHdr*)pThisData)->extraPtr = newNode;

        // 调整next leaf
        if(tempNode != IX_INVALID_NODE)
        {
            if((rc = pfFh.GetThisPage(tempNode, ph))  ||
               (rc = ph.GetData(pTemp)))
                return (rc);

            ((IX_NodeHdr*)pTemp)->prevPtr = newNode;
            
            if((rc = pfFh.MarkDirty(tempNode))  ||
               (rc = pfFh.UnpinPage(tempNode)))
                return (rc);
        }
    }

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(thisNode))  ||
       (rc = pfFh.UnpinPage(thisNode))  ||
       (rc = pfFh.MarkDirty(newNode))   ||
       (rc = pfFh.UnpinPage(newNode)))
        return (rc);

    return (OK_RC);
}

//
// BinarySearch
//
// Desc: 对 内部节点 或 叶节点 进行二分查找，并
//       返回 最佳插入位置 和 targetKey对应的ptr (即childNode)。
//       查找过程将寻找不大于targetKey的最大key位置。
//       对于leaf可能查找失败，则返回无效pageNum。 TODO 优化计算过程
// In:   pTargetKey - 指向待查找键值的指针。
//       thisNode   - 待查找node的pageNum。
// Out:  pos        - 等价于按数组查找时的下标。
//       childNode  - targetKey对应的ptr。
// Ret:  IX return code
//
RC IX_IndexHandle::BinarySearch(void *pTargetKey, const PageNum thisNode, int &pos, PageNum &childNode) const
{
    // 参数检查
    if((!pTargetKey) || (thisNode == IX_INVALID_NODE))
        return (IX_SEARCHFAILED);

    RC rc;
    PF_PageHandle ph;
    char *pData;

    // 读取Node信息
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
       (rc = ph.GetData(pData)))
        return (rc);

    int keyNum = ((IX_NodeHdr*)pData)->keyNum;
    int level  = ((IX_NodeHdr*)pData)->level;

    // 待查找node不能为空
    if (keyNum == 0)
    {
        pfFh.UnpinPage(thisNode);
        return (IX_SEARCHEMPTYNODE);
    }

    int nodeHdrSize = sizeof(IX_NodeHdr);
    int entryLength = hdr.attrLength + 4;       // key-pointer对长度
    int start, mid, end;
    start = 0;
    end = keyNum - 1;

    // 二分查找不大于target的最大key
    while(start < end)  // 当 start == end 时跳出循环
    {
        mid = (start + end + 1) / 2;      // 向上取整
        pos = nodeHdrSize + mid * entryLength;

        if(LessThanOrEqual(pData + pos, pTargetKey, hdr.attrType, hdr.attrLength))
        {   // key <= target
            start = mid;
        }
        else
        {   // key > target
            end   = mid - 1;
        }
    }

    // 记录位置
    pos = nodeHdrSize + start * entryLength;

    // 得到返回值 (pos, childNode)
    if(level == IX_LEAF_LEVEL)
    {
        if(Equal(pData + pos, pTargetKey, hdr.attrType, hdr.attrLength))
        {
            childNode = *(PageNum*)(pData + pos + hdr.attrLength);
        }
        else if(LessThan(pData + pos, pTargetKey, hdr.attrType, hdr.attrLength))
        {   // key < target
            pos += entryLength;
            childNode = IX_INVALID_NODE;
        }
        else// key > target
        {
            childNode = IX_INVALID_NODE;
        }
    }
    else if(level > IX_LEAF_LEVEL)
    {
        // 判断是否找到
        if(LessThanOrEqual(pData + pos, pTargetKey, hdr.attrType, hdr.attrLength))
        {   // key <= target
            childNode = *(PageNum*)(pData + pos + hdr.attrLength);
            pos += entryLength;
        }
        else
        {   // key > target，比最小key更小，返回extra指针
             childNode = ((IX_NodeHdr*)pData)->extraPtr;
        }
    }

    // unpin
    if(rc = pfFh.UnpinPage(thisNode))
       return (rc);

    return (OK_RC);
}

//
// InsertNode
//
// Desc: 在thisNode上的pos处插入(key, childNode)，并更新NodeHdr。
// In:   pKey    - 待查找的键值。
//       level   - B+树中节点的层次。level > IX_LEAF_LEVEL。
// Ret:  IX return code.
//
RC IX_IndexHandle::InsertNode(void *pKey, PageNum thisNode, PageNum childNode , int pos)
{
    RC rc;
    PF_PageHandle ph;
    char *pData;

    // 读取Node信息
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
       (rc = ph.GetData(pData)))
        return (rc);

    // Node必须未满
    if(((IX_NodeHdr*)pData)->keyNum >= hdr.keyNumPerPage)
    {    
        pfFh.UnpinPage(thisNode);
        return (IX_INSERTNODEFILED);
    }

    // 若不为最后一个，则腾出位置
    int entryLength = hdr.attrLength + 4;
    int moveSize = sizeof(IX_NodeHdr) + entryLength * ( ((IX_NodeHdr*)pData)->keyNum ) - pos;
    if(moveSize != 0)
        memmove(pData + pos + entryLength, pData + pos, moveSize);

    // pos处插入(pKey, childNode)
    memcpy(pData + pos, pKey, hdr.attrLength);
    memcpy(pData + pos + hdr.attrLength, &childNode, 4);

    // 更新hdr
    ((IX_NodeHdr*)pData)->keyNum++;

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(thisNode))   ||
       (rc = pfFh.UnpinPage(thisNode)))
       return (rc);   
    
    return (OK_RC);
}

//
// InsertBucket
//
// Desc: 在bucket中寻找空闲位置并插入rid。若所有bucket已满，则新建bucket后插入
// In:   thisNode - 待查找的bucket。
//       rid      - 待插入的rid
// Ret:  IX return code.
//
RC IX_IndexHandle::InsertBucket(PageNum thisNode, const RID &rid)
{
    RC rc;
    PF_PageHandle ph;
    char *pData, *pOldData;
    int pos;
    PageNum newBucket;

    // 读取Node信息
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
       (rc = ph.GetData(pData)))
        return (rc);

    // node类型判断
    if(((IX_BucketHdr*)pData)->level != IX_BUCKET_LEVEL)
    {    
        pfFh.UnpinPage(thisNode);
        return (IX_INSERTBUCKETFILED);
    }

    // 找到一个有空间的bucket，若整个链上bucket全满，则新建空Bucket
    while(((IX_BucketHdr*)pData)->ridNum >= hdr.ridNumPerPage)
    {
        // 当前bucket无空间，寻找新的空间
        if(((IX_BucketHdr*)pData)->nextPtr != IX_INVALID_NODE)
        {   // 找下一个

            newBucket = ((IX_BucketHdr*)pData)->nextPtr;

            // 读取Node信息
            if((rc = pfFh.GetThisPage(newBucket, ph))    ||
               (rc = ph.GetData(pData)))
                return (rc);
        }
        else
        {   // 需要新生成Bucket

            pOldData = pData;

            // 新建bucket，并读取其信息
            if((rc = CreateBucket(newBucket))         ||
            (rc = pfFh.GetThisPage(newBucket, ph))    ||
            (rc = ph.GetData(pData)))
                return (rc);
            
            // 将新bucket与当前bucket连接
            ((IX_BucketHdr*)pData)->nextPtr = ((IX_BucketHdr*)pOldData)->nextPtr;
            ((IX_BucketHdr*)pOldData)->nextPtr = newBucket;
            ((IX_BucketHdr*)pData)->prevPtr = thisNode;

            // set dirty
            if(rc = pfFh.MarkDirty(thisNode))
                return (rc);
        }
    
        // unpin
        if(rc = pfFh.UnpinPage(thisNode))
            return (rc);

        thisNode = newBucket;
    }

    int ridEntrySize = sizeof(IX_RidEntry);
    pos = ((IX_BucketHdr*)pData)->freeList;

    // 插入rid
    ((IX_RidEntry*)(pData + pos))->rid = rid;

    // 调整freeList、useList
    ((IX_BucketHdr*)pData)->freeList = ((IX_RidEntry*)(pData + pos))->next;
    ((IX_RidEntry*)(pData + pos))->next = ((IX_BucketHdr*)pData)->useList;
    ((IX_BucketHdr*)pData)->useList = pos;

    // 更新rid计数
    ((IX_BucketHdr*)pData)->ridNum++;

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(thisNode))   ||
       (rc = pfFh.UnpinPage(thisNode)))
       return (rc);

    return (OK_RC);
}

//
// FindRebalance
//
// Desc: 递归向下找到需要删除的节点，在寻找过程中记录anchor节点和邻居节点。
//       到达leaf后，删除上面的entry(或者rid)。删除后根据节点是否underflow执行调整。
// In:   done      - 递归向下时，作为待删除的node
//       thisNode  - 当前node
//       leftNode  - thisNode的左邻居
//       rightNode - thisNode的右邻居
//       lAnchor   - thisNode与左邻居之间的anchor
//       rAnchor   - thisNode与右邻居之间的anchor
//       pKey      - 指向待删除key的指针
//       rid       - 待插入的rid
// Out:  done      - 用于解递归时候将删除信息传递给上层。
// Ret:  IX return code.
//
RC IX_IndexHandle::FindRebalance(PageNum &done,     PageNum thisNode, PageNum leftNode, 
                                 PageNum rightNode, PageNum lAnchor,  PageNum rAnchor, void *pKey, const RID &rid)
{
    // 局部变量
    RC rc;
    PageNum removeNode = IX_INVALID_NODE, nextNode;
    PageNum nextLeft, nextRight, nextAncL, nextAncR;
    PF_PageHandle ph;
    char* pData;
    int pos, entryLength = hdr.attrLength + 4;

    //////////////////////////////////////////////////////////////////////////////
    ///               从root递归向下到leaf，寻找需要rebalance的node                ///
    //////////////////////////////////////////////////////////////////////////////

    // 获取当前node信息
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
    (rc = ph.GetData(pData)))
        return (rc);

    // 计算underflow边界
    int minNum = (thisNode == hdr.root) ? 1 : (hdr.keyNumPerPage / 2);
    if(((IX_NodeHdr*)pData)->keyNum > minNum)
        balanceNode = IX_INVALID_NODE;
    else if(balanceNode == IX_INVALID_NODE)
        balanceNode = thisNode;

    // 查找当前Node的孩子
    if(rc = BinarySearch(pKey, thisNode, pos, nextNode))
        return (rc);

    // 没有到达leaf，则继续查找
    if(((IX_NodeHdr*)pData)->level != IX_LEAF_LEVEL)
    {   // 计算neight和anchor节点
        
        char *pTmepData;
        int tempOffset;

        if(pos == sizeof(IX_NodeHdr))
        {   // nextNode是thisNode中extraPtr，则前面无entry
            // 只能记录左邻居中最大的entry对应childNode
            // 注：pos指的是最佳插入位置
            
            nextAncL = lAnchor;
            nextLeft = IX_INVALID_NODE;
            if(leftNode != IX_INVALID_NODE)
            {
                if((rc = pfFh.GetThisPage(leftNode, ph)) ||
                   (rc = ph.GetData(pTmepData)))
                   return (rc);

                tempOffset = sizeof(IX_NodeHdr)
                           + entryLength * (((IX_NodeHdr*)pTmepData)->keyNum - 1)
                           + hdr.attrLength;
                
                memcpy(&nextLeft, pTmepData + tempOffset, 4);

                if(rc = pfFh.UnpinPage(leftNode))
                    return (rc);
            }
        }
        else// 获取nextNode在thisNode中前一个entry
        {
            nextAncL = thisNode;
            pLeftAnchorKey = pData + pos - entryLength;
            leftAnchor = thisNode;
            
            if(pos - entryLength == sizeof(IX_NodeHdr))
                nextLeft = ((IX_NodeHdr*)pData)->extraPtr;
            else
                nextLeft = *(PageNum*)(pData + pos - entryLength - 4);
        }

        if(pos == sizeof(IX_NodeHdr) + ((IX_NodeHdr*)pData)->keyNum * entryLength)
        {   // nextNode是thisNode中最大entry，则后面无entry
            // 只能记录右邻居中最小的entry对应childNode

            nextAncR = rAnchor;
            nextRight = IX_INVALID_NODE;
            if(rightNode != IX_INVALID_NODE)
            {
                if((rc = pfFh.GetThisPage(rightNode, ph)) ||
                   (rc = ph.GetData(pTmepData)))
                   return (rc);
                
                nextRight = ((IX_NodeHdr*)pTmepData)->extraPtr;

                if(rc = pfFh.UnpinPage(rightNode))
                    return (rc);
            }
        }
        else// 获取nextNode在thisNode中后一个entry
        {
            nextAncR = thisNode;
            nextRight = *(PageNum*)(pData + pos + hdr.attrLength);
        }

        // 递归调用
        if(rc = FindRebalance(removeNode, nextNode, nextLeft, nextRight, nextAncL, nextAncR, pKey, rid))
            return (rc);
    }
    else    // key是否找到
    {
        // if(nextNode != IX_INVALID_NODE)
        //     removeNode = nextNode;
        // else
        //     removeNode = IX_INVALID_NODE;
        removeNode = nextNode;
    }

    //////////////////////////////////////////////////////////////////////////////
    ///             删除pKey、解递归、rebalance、从当前node中删除entry              ///
    //////////////////////////////////////////////////////////////////////////////
    
    // 先释放下级空间
    if(removeNode != IX_INVALID_NODE)
    {   
        // 若为叶结点，则删除removeNode(Bucket)中的rid
        if(((IX_NodeHdr*)pData)->level == IX_LEAF_LEVEL)
        {    
            if(rc = DeleteBucket(removeNode, rid))
                return (rc);
        }
        else    // 非叶节点，则释放下级空间
        {
            if(rc = pfFh.DisposePage(removeNode))
                return (rc);

            removeNode = IX_INVALID_NODE; 
        }
        
        // 若下层 Bucket/普通节点 全部被删除，则删除thisNode上的对应entry
        if(removeNode == IX_INVALID_NODE)
        {   // 根据pos删除thisNode上entry...
            
            // 调整内部结点位置
            if(((IX_NodeHdr*)pData)->level != IX_LEAF_LEVEL)
            {   
                if(pos > sizeof(IX_NodeHdr)) 
                    pos -= entryLength;
            }
            // 更新lAnchor中的分隔entry的key
            else if(( pos == sizeof(IX_NodeHdr) )   &&      // leaf中最小entry
                    ( pLeftAnchorKey != NULL )      &&      // 存在lAnchor
                    ( Equal(pLeftAnchorKey, pData + pos, hdr.attrType, hdr.attrLength) ))   // TODO 该判断是否冗余？
            {   // thisNode 为 leaf...

                // 找到删除完成后leaf中新的entry，将其key复制到lAnchor中分隔位上的entry中
                // 仅在 degree < 4 时不成立
                if( ((IX_NodeHdr*)pData)->keyNum > 1 )
                    memcpy(pLeftAnchorKey, pData + pos + entryLength, hdr.attrLength);

                if(rc = pfFh.MarkDirty(leftAnchor))
                    return (rc);
            }

            // 覆盖位置
            int offset = sizeof(IX_NodeHdr) + entryLength * ( ((IX_NodeHdr*)pData)->keyNum );
            if(pos + entryLength < offset)
                memmove(pData + pos, pData + pos + entryLength, offset - pos - entryLength);

            // 更新hdr
            ((IX_NodeHdr*)pData)->keyNum--;
                
            // make dirty
            if(rc = pfFh.MarkDirty(thisNode))
                return (rc);
        }
    }

    int thisLevel = ((IX_NodeHdr*)pData)->level;
    // unpin，前面子函数中包含了make dirty
    if(rc = pfFh.UnpinPage(thisNode))
        return (rc);

    // 删除结束后，检查thisNode需要哪种rebalance
    if(balanceNode == IX_INVALID_NODE)
    {   // 节点entry数量符合半满要求...

        done = IX_INVALID_NODE; 
    }    
    else if(thisLevel == hdr.height)
    {   // 刚删除了root上的最后一个entry...

        if(rc = CollapseRoot(done, thisNode))
            return (rc);
    }
    else// 低于半满要求...
    {
        if(rc = Rebalance(done, thisNode, leftNode, rightNode, lAnchor, rAnchor))
            return (rc);
    }

    return (OK_RC);
}

//
// DeleteBucket
//
// Desc: 删除Bucket上的rid。先寻找rid，找不到返回NO_NODE;找到则删除。
//       删除后bucket空则回收bucket; 全部bucket均为空则设置返回值为NO_NODE
// In:   thisBucket - 带删除rid所在bucket。
//       rid        - 待删除rid。
// Out:  thisBucket - 指示是否释放所有bucket
// Ret:  IX return code.
//
RC IX_IndexHandle::DeleteBucket(PageNum &thisBucket, const RID &rid)
{
    RC rc;
    PF_PageHandle ph;
    char *pData;
    int pos;

    // 找到rid
    if(rc = FindRid(pos, thisBucket, rid))
        return (rc);
    
    // 未找到，返回
    if(thisBucket == IX_INVALID_NODE)
        return (OK_RC);

    // 读取Node信息
    if((rc = pfFh.GetThisPage(thisBucket, ph))    ||
       (rc = ph.GetData(pData)))
        return (rc);

    // 删除pos位置上的rid，并调整list
    ((IX_BucketHdr*)pData)->useList = ((IX_RidEntry*)(pData + pos))->next;
    ((IX_RidEntry*)(pData + pos))->next = ((IX_BucketHdr*)pData)->freeList;
    ((IX_BucketHdr*)pData)->freeList = pos;
    ((IX_BucketHdr*)pData)->ridNum--;   // 数量减少。

    int ridNum = ((IX_BucketHdr*)pData)->ridNum;    
    PageNum prevBucket, nextBucket;

    // 若Bucekt空，则dispose
    // 先读入前后bucket（若有），然后改一下，然后mkdirty，unpin
    // 或者分别改动前后Bucket，分别mkdirty、unpin
    // 然后unpin，最后dispose
        // 同时，若前后都没bucket了，则待返回的remNode = NO_NODE
    if(ridNum == 0)
    {
        prevBucket = ((IX_BucketHdr*)pData)->prevPtr;
        nextBucket = ((IX_BucketHdr*)pData)->nextPtr;
        char *pTempData;

        // 更新nextBucket中指针
        if(nextBucket != IX_INVALID_NODE)
        {
            // 读取Node信息
            if((rc = pfFh.GetThisPage(nextBucket, ph))    ||
            (rc = ph.GetData(pTempData)))
                return (rc);

            ((IX_BucketHdr*)pTempData)->prevPtr = prevBucket;

            // set dirty、unpin
            if((rc = pfFh.MarkDirty(nextBucket))   ||
            (rc = pfFh.UnpinPage(nextBucket)))
                return (rc);
        }

        // 更新prevBucket中指针
        if(prevBucket != IX_INVALID_NODE)
        {
            // 读取Node信息
            if((rc = pfFh.GetThisPage(prevBucket, ph))    ||
            (rc = ph.GetData(pTempData)))
                return (rc);

            ((IX_BucketHdr*)pTempData)->nextPtr = nextBucket;

            // set dirty、unpin
            if((rc = pfFh.MarkDirty(prevBucket))   ||
            (rc = pfFh.UnpinPage(prevBucket)))
                return (rc);
        }
    }

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(thisBucket))   ||
        (rc = pfFh.UnpinPage(thisBucket)))
        return (rc);  

    if(ridNum == 0)
    {
        if(rc = pfFh.DisposePage(thisBucket))
            return (rc);
    }  

    if(prevBucket == IX_INVALID_NODE && nextBucket == IX_INVALID_NODE)
    {       // 当前bucket为最后一个...
        thisBucket = IX_INVALID_NODE;
    }

    return (OK_RC);
}

//
// FindRid
//
// Desc: 从leaf中entry指向的bucket开始遍历，寻找符合条件的rid所在的bucket和位置。
// In:   thisBucket - 查找起始处的bucket
//       rid        - 带查找rid
// Out:  pos        - rid在bucket上的位置。找不到返回IX_RID_LIST_END。
//       thisBucket - rid所在bucket。找不到为IX_INVALID_NODE。
// Ret:  IX return code.
//
RC IX_IndexHandle::FindRid(int &pos, PageNum &thisBucket, const RID &rid)
{
    RC rc;
    PF_PageHandle ph;
    char *pData;
    PageNum nextBucket;

    if(thisBucket == IX_INVALID_NODE)
        return (IX_FINDRIDFILED);

    do{
        // 读取Node信息
        if((rc = pfFh.GetThisPage(thisBucket, ph))    ||
           (rc = ph.GetData(pData)))
            return (rc);

        // assert(((IX_BucketHdr*)pData)->ridNum > 0)

        // 初始化
        pos = ((IX_BucketHdr*)pData)->useList;

        // 搜索当前Bucket
        while (pos != IX_RID_LIST_END)
        {
            if (((IX_RidEntry*)(pData + pos))->rid == rid)
                break;
            else
                pos = ((IX_RidEntry*)(pData + pos))->next;
        }

        nextBucket = ((IX_BucketHdr*)pData)->nextPtr;
        
        // unpin
        if(rc = pfFh.UnpinPage(thisBucket))
        return (rc);   
        
        // 更新thisBucket
        if(pos == IX_RID_LIST_END)
        {
            thisBucket = nextBucket;
            
            if(nextBucket == IX_INVALID_NODE)
                break;
        }     

    }while(pos == IX_RID_LIST_END);

    return (rc);
}

//
// CollapseRoot
//
// Desc: 收缩根节点，并改动hdr中根的位置和树高参数。
// In:   thisNode - 待收缩的根节点
// Out:  newNode  - 新的根节点
// Ret:  IX return code.
//
RC IX_IndexHandle::CollapseRoot(PageNum &newRoot, PageNum thisNode)
{
    RC rc;
    PF_PageHandle ph;
    char* pData; 

    // 读取数据
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
       (rc = ph.GetData(pData)))
        return (rc);

    if(((IX_NodeHdr*)pData)->level == IX_LEAF_LEVEL)   
        newRoot = IX_INVALID_NODE;                  // B+tree 为空
    else
        hdr.root = ((IX_NodeHdr*)pData)->extraPtr;  // 只剩最后一个extraPtr

    // 更新hdr
    hdr.height--;
    bHdrChanged = TRUE;

    // unpin、dispose
    if((rc = pfFh.UnpinPage(thisNode))  ||
       (rc = pfFh.DisposePage(thisNode)))
        return (rc);

    return (rc);
}

//
// Rebalance
//
// Desc: 根据邻居节点上entry执行shift或者merge操作。优先考虑shift操作。
// In:   thisNode  - 当前node
//       leftNode  - thisNode的左邻居
//       rightNode - thisNode的右邻居
//       lAnchor   - thisNode与左邻居之间的anchor
//       rAnchor   - thisNode与右邻居之间的anchor
// Out:  done      - 返回给递归函数处理的pageNode
// Ret:  IX return code.
//
RC IX_IndexHandle::Rebalance(PageNum &done,     PageNum thisNode, PageNum leftNode,
                             PageNum rightNode, PageNum lAnchor,  PageNum rAnchor)
{
    RC rc;
    PageNum anchorNode, mergeNode, tempNode;
    IX_NodeHdr thisHdr, leftNodeHdr, rightNodeHdr, balanHdr;
    PF_PageHandle ph;
    char *pData;
    int pos;

    // 读取thisNode的Hdr信息
    if((rc = pfFh.GetThisPage(thisNode, ph))    ||
       (rc = ph.GetData(pData)))
       return (rc);
    
    thisHdr = *(IX_NodeHdr*)pData;

    if(rc = pfFh.UnpinPage(thisNode))
        return (rc);

    // 读取leftNode的Hdr信息
    if(leftNode != IX_INVALID_NODE)
    {    
        if((rc = pfFh.GetThisPage(leftNode, ph))    ||
            (rc = ph.GetData(pData)))
            return (rc);
        
        leftNodeHdr = *(IX_NodeHdr*)pData;

        if(rc = pfFh.UnpinPage(leftNode))
            return (rc);
    }

    // 读取rightNode的Hdr信息
    if(rightNode != IX_INVALID_NODE)
    {    
        if((rc = pfFh.GetThisPage(rightNode, ph))    ||
            (rc = ph.GetData(pData)))
            return (rc);
        
        rightNodeHdr = *(IX_NodeHdr*)pData;

        if(rc = pfFh.UnpinPage(rightNode))
            return (rc);
    }

    // 选择邻居中有更多entries的那个
    if(leftNode != IX_INVALID_NODE)
        balanceNode = leftNode;

    if(rightNode != IX_INVALID_NODE)
    {
        if(leftNode == IX_INVALID_NODE)
            balanceNode = rightNode;
        else if(leftNodeHdr.keyNum < rightNodeHdr.keyNum)
            balanceNode = rightNode;            
    }

    if(balanceNode == leftNode)
        balanHdr = leftNodeHdr;
    else
        balanHdr = rightNodeHdr;

    // 根据balanceNode上entries数量选择调整形式
    if(balanHdr.keyNum > hdr.keyNumPerPage / 2)
    {   // 执行shift...

        if(balanceNode == leftNode)
            anchorNode = lAnchor;
        else
            anchorNode = rAnchor;

        Shift(done, thisNode, balanceNode, anchorNode);
    }
    else// 执行merge...
    {
        int lAnchorLevel, rAnchorLevel;

        // 读取leftAnchor的level
        if(lAnchor != IX_INVALID_NODE)
        {    
            if((rc = pfFh.GetThisPage(lAnchor, ph))    ||
            (rc = ph.GetData(pData)))
            return (rc);
            
            lAnchorLevel = (*(IX_NodeHdr*)pData).level;

            if(rc = pfFh.UnpinPage(lAnchor))
                return (rc);
        }

        // 读取rightAnchor的level
        if(rAnchor != IX_INVALID_NODE)
        {    
            if((rc = pfFh.GetThisPage(rAnchor, ph))    ||
            (rc = ph.GetData(pData)))
            return (rc);
            
            rAnchorLevel = (*(IX_NodeHdr*)pData).level;

            if(rc = pfFh.UnpinPage(rAnchor))
                return (rc);
        }

        // 找thisNode的parent(均为parent则选左边)
        if(lAnchor != IX_INVALID_NODE)
            anchorNode = lAnchor;
        
        if(rAnchor != IX_INVALID_NODE)
        {
            if(lAnchor == IX_INVALID_NODE)
                anchorNode = rAnchor;
            else if(lAnchorLevel > rAnchorLevel)
                anchorNode = rAnchor;
        }

        if(anchorNode == lAnchor)
            mergeNode = leftNode;
        else
            mergeNode = rightNode;

        Merge(done, thisNode, mergeNode, anchorNode);
    }

    return (rc);
}

//
// Shift
//
// Desc: 从邻居节点上借过来entry，使得最终二者entry数目相当。
// In:   
//       
// Out:
// Ret:  IX return code.
//
RC IX_IndexHandle::Shift(PageNum &done, PageNum thisNode, PageNum neighborNode, PageNum anchorNode)
{
    RC rc;
    PF_PageHandle ph;
    char *pThisData, *pNeighborData, *pAnchorData;
    int entryLength = hdr.attrLength + 4;
    int nodeHdrSize = sizeof(IX_NodeHdr);

    // 获得Node信息
    if((rc = pfFh.GetThisPage(thisNode, ph))    || 
       (rc = ph.GetData(pThisData))         ||
       (rc = pfFh.GetThisPage(neighborNode, ph))||
       (rc = ph.GetData(pNeighborData))         ||
       (rc = pfFh.GetThisPage(anchorNode, ph))  ||
       (rc = ph.GetData(pAnchorData)))
       return (rc);

    // 判断thisNode是否在anchor右边
    bool bRight = GreaterThan(pThisData + nodeHdrSize, 
                              pNeighborData + nodeHdrSize, 
                              hdr.attrType, hdr.attrLength);
    
    // 获得anchor中分隔key的位置
    char* pTargetKey;
    if(bRight)
        pTargetKey = pThisData + nodeHdrSize;
    else
        pTargetKey = pNeighborData + nodeHdrSize;
    
    int pos;    // 右边节点对应的上层entry
    PageNum tempNode;
    if(rc = BinarySearch(pTargetKey, anchorNode, pos, tempNode))
        return (rc);
    pos -= entryLength; // anchor一定不是leaf，总需要调整

    int thisLevel   = ((IX_NodeHdr*)pThisData  )->level;
    int anchorLevel = ((IX_NodeHdr*)pAnchorData)->level;

    // 若thisNode为内部节点，将anchorNode上的分隔值copy给thisNode
    // 注：将新key和extraPtr组成新entry
    if (thisLevel != IX_LEAF_LEVEL)
    {
        int offset, moveSize;

        if(bRight)
        {   // thisNode在Anchor右边...

            // 腾出位置
            offset   = nodeHdrSize;
            moveSize = ((IX_NodeHdr*)pThisData)->keyNum * entryLength;
            memmove(pThisData + offset + entryLength, pThisData + offset, moveSize);
        
            // 写入
            memcpy(pThisData + offset, pAnchorData + pos, hdr.attrLength);
            *(PageNum*)(pThisData + offset + hdr.attrLength) = ((IX_NodeHdr*)pThisData)->extraPtr;
        }
        else// thisNode在Anchor左边...
        {
            offset = nodeHdrSize + ((IX_NodeHdr*)pThisData)->keyNum * entryLength;
            
            // 直接写入末尾
            memcpy(pThisData + offset, pAnchorData + pos, hdr.attrLength);
            *(PageNum*)(pThisData + offset + hdr.attrLength) = ((IX_NodeHdr*)pNeighborData)->extraPtr;
        }

        ((IX_NodeHdr*)pThisData)->keyNum++;
    }
    
    char *pMidEntry;
    int midPos, shiftlength, numDiff;
    int numGet, numThis, numNeighbor;   // 调整后两个节点中entry数量

    // 进行调整，使得thisNode和neighbor上entry尽量相等      TOTO 改进写法，变量使用等
    if(bRight)
    {
        // 计算调整后两节点entry个数
        midPos = ((IX_NodeHdr*)pThisData)->keyNum + ((IX_NodeHdr*)pNeighborData)->keyNum;
        numNeighbor = midPos / 2;   // 先保证左边node获得一半entry(或略少)
        numThis = midPos - numNeighbor; // 动态调整发生在右边

        // 计算neighbor上分隔位置处的entry地址和偏移
        midPos = nodeHdrSize + numNeighbor * entryLength;
        pMidEntry = pNeighborData + midPos;   // 地址

        // 将分隔处entry复制进anchor（相当于转完后改动lAnchor）
        // memcpy(pLeftAnchorKey, pMidEntry, hdr.attrLength);   // TODO 考虑使用pLeftAnchorKey减少search
        memcpy(pAnchorData + pos, pMidEntry, hdr.attrLength);   // 需更新lAnchor对应key

        // 若为内部节点，则不必拷贝mid，而是更改extra
        if(thisLevel != IX_LEAF_LEVEL)
        {    
            ((IX_NodeHdr*)pThisData)->extraPtr = *(PageNum*)(pMidEntry + hdr.attrLength);
            midPos += entryLength;
            --numThis;
        }
        
        // thisNode腾出空间
        shiftlength = (numThis - ((IX_NodeHdr*)pThisData)->keyNum) * entryLength;
        memmove(pThisData + nodeHdrSize + shiftlength, 
                pThisData + nodeHdrSize,  shiftlength);

        // 将neighbor上entry调整到thisNode
        memcpy(pThisData + nodeHdrSize, pNeighborData + midPos, shiftlength);
    }
    else
    {
        // 计算调整后两节点entry个数
        midPos = ((IX_NodeHdr*)pThisData)->keyNum + ((IX_NodeHdr*)pNeighborData)->keyNum;
        numThis = midPos / 2;           // 先保证左边node获得一半entry(或略少)
        numNeighbor = midPos - numThis; // 动态调整发生在右边

        // 计算neighbor上分隔后，将处于首位的entry此时的地址和偏移
        numDiff = numThis - ((IX_NodeHdr*)pThisData)->keyNum;
        midPos = nodeHdrSize + numDiff * entryLength;   // 偏移
        pMidEntry = pNeighborData + midPos;       // 地址

        // 将分隔处entry复制进anchor（相当于转完后改动rAnchor）
        memcpy(pAnchorData + pos, pMidEntry, hdr.attrLength);

        // 若为内部节点，则不必拷贝mid，而是更改extra
        if(thisLevel != IX_LEAF_LEVEL)
        {
            ((IX_NodeHdr*)pNeighborData)->extraPtr = *(PageNum*)(pMidEntry + hdr.attrLength);
            midPos += entryLength;
            --numNeighbor;
        }

        // 将neighbor上entry调整到thisNode
        shiftlength = numDiff * entryLength;
        memcpy(pThisData + nodeHdrSize + entryLength * ((IX_NodeHdr*)pThisData)->keyNum, 
               pNeighborData + nodeHdrSize, shiftlength);

        // 调整neighbor上空间
        shiftlength = numNeighbor * entryLength;
        memmove(pNeighborData + nodeHdrSize, 
                pNeighborData + midPos,  shiftlength);

        // 若当前层次为leaf，且存在lAnchor，
        // 且this中所有entry都是借来的（仅在entry最大数目小于4时发生）
        // 则需要额外更新其父节点中的分隔值
        if(((numThis == numDiff))       &&      // 说明位于左侧的this节点shift前为空
           (thisLevel == IX_LEAF_LEVEL) &&
           (pLeftAnchorKey != NULL))
        {
            memcpy(pLeftAnchorKey, pThisData + nodeHdrSize, hdr.attrLength);
            pLeftAnchorKey = NULL;      // 冗余？

            if(rc = pfFh.MarkDirty(leftAnchor))
                return (rc);
        }
    }

    // 改动hdr
    ((IX_NodeHdr*)pThisData)->keyNum = numThis;
    ((IX_NodeHdr*)pNeighborData)->keyNum = numNeighbor;

    // set dirty、unpin
    if((rc = pfFh.MarkDirty(thisNode))    || 
       (rc = pfFh.UnpinPage(thisNode))    ||
       (rc = pfFh.MarkDirty(neighborNode))||
       (rc = pfFh.UnpinPage(neighborNode))||
       (rc = pfFh.MarkDirty(anchorNode))  ||
       (rc = pfFh.UnpinPage(anchorNode)))
       return (rc);

    balanceNode = IX_INVALID_NODE;

    return (OK_RC);
}

//
// Merge
//
// Desc: 将thisNode上entry全部转移到邻居节点上。
// In:   
//       
// Out:
// Ret:  IX return code.
//
RC IX_IndexHandle::Merge(PageNum &done, PageNum thisNode, PageNum neighborNode, PageNum anchorNode)
{
    RC rc;
    PF_PageHandle ph;
    char *pThisData, *pNeighborData, *pAnchorData;
    int entryLength = hdr.attrLength + 4;
    int nodeHdrSize = sizeof(IX_NodeHdr);

    // 获得Node信息
    if((rc = pfFh.GetThisPage(thisNode, ph))    || 
       (rc = ph.GetData(pThisData))             ||
       (rc = pfFh.GetThisPage(neighborNode, ph))||
       (rc = ph.GetData(pNeighborData))         ||
       (rc = pfFh.GetThisPage(anchorNode, ph))  ||
       (rc = ph.GetData(pAnchorData)))
       return (rc);

    // 判断thisNode是否在anchor右边
    bool bRight = GreaterThan(pThisData + nodeHdrSize, 
                              pNeighborData + nodeHdrSize, 
                              hdr.attrType, hdr.attrLength);
    
    // 获得anchor中分隔key的位置
    char* pTargetKey;
    if(bRight)
        pTargetKey = pThisData + nodeHdrSize;
    else
        pTargetKey = pNeighborData + nodeHdrSize;
    
    int pos;
    PageNum tempNode;
    if(rc = BinarySearch(pTargetKey, anchorNode, pos, tempNode))
        return (rc);
    pos -= entryLength;

    // 若thisNode为内部节点，将 anchorNode 上的分隔值copy给neighbor
    // 注：将新key和extraPtr组成新entry
    int offset, moveSize;
    if (((IX_NodeHdr*)pThisData)->level != IX_LEAF_LEVEL)
    {
        if(bRight)
        {   // thisNode在Anchor右边...
            offset = nodeHdrSize + ((IX_NodeHdr*)pNeighborData)->keyNum * entryLength;
            
            // 直接写入末尾
            memcpy(pNeighborData + offset, pAnchorData + pos, hdr.attrLength);
            *(PageNum*)(pNeighborData + offset + hdr.attrLength) = ((IX_NodeHdr*)pThisData)->extraPtr;
        }
        else// thisNode在Anchor左边...
        {
            // neighbor所有entry向后移动一格
            offset   = nodeHdrSize;
            moveSize = ((IX_NodeHdr*)pNeighborData)->keyNum * entryLength;
            memmove(pNeighborData + offset + entryLength, pNeighborData + offset, moveSize);
        
            // 写入
            memcpy(pNeighborData + offset, pAnchorData + pos, hdr.attrLength);
            *(PageNum*)(pNeighborData + offset + hdr.attrLength) = ((IX_NodeHdr*)pNeighborData)->extraPtr;
        
            // 拷贝extra指针
            ((IX_NodeHdr*)pNeighborData)->extraPtr = ((IX_NodeHdr*)pThisData)->extraPtr;
        }

        ((IX_NodeHdr*)pNeighborData)->keyNum++;
    }

    // 将thisNode上剩余entry转移到neighbor上
    if(bRight)
    {
        // 移动
        offset = nodeHdrSize + ((IX_NodeHdr*)pNeighborData)->keyNum * entryLength;
        moveSize = ((IX_NodeHdr*)pThisData)->keyNum * entryLength;
        memcpy(pNeighborData + offset, pThisData + nodeHdrSize, moveSize);
    }
    else
    {
        // neighbor腾出空间
        offset = nodeHdrSize + ((IX_NodeHdr*)pThisData)->keyNum * entryLength;
        moveSize = ((IX_NodeHdr*)pNeighborData)->keyNum * entryLength;
        memmove(pNeighborData + offset, pNeighborData + nodeHdrSize, moveSize);
        
        // 移动
        moveSize = ((IX_NodeHdr*)pThisData)->keyNum * entryLength;
        memcpy(pNeighborData + nodeHdrSize, pThisData + nodeHdrSize, moveSize);
    
        // 若thisNode被父节点的extra指向，则需要改变父指针
        // 注：上层可以保证pos满足：若this被extra指向，则删除neighbor的entry
        if(pos == nodeHdrSize)
        {
            ((IX_NodeHdr*)pAnchorData)->extraPtr = neighborNode;    // 无论是否为leaf
        
            // 当degree < 4 时要考虑（极少情况）
            // 此时上层分隔entry的key需要用neighbor中最小entry更新
            if((moveSize == 0)          &&
               (pLeftAnchorKey != NULL) &&
               (((IX_NodeHdr*)pThisData)->level == IX_LEAF_LEVEL))
            {
                memcpy(pLeftAnchorKey, pNeighborData + nodeHdrSize, hdr.attrLength);
                pLeftAnchorKey = NULL;      // 冗余？

                if(rc = pfFh.MarkDirty(leftAnchor))
                    return (rc);
            }
        }
        // 若为leaf，考虑更新上层分隔entry中key
        else if(((IX_NodeHdr*)pThisData)->level == IX_LEAF_LEVEL)
            memcpy(pAnchorData + pos, pNeighborData + nodeHdrSize, hdr.attrLength);    
    }

    // 更改hdr
    ((IX_NodeHdr*)pNeighborData)->keyNum += ((IX_NodeHdr*)pThisData)->keyNum;

    // 若为leaf，需调整双向指针
    if(((IX_NodeHdr*)pNeighborData)->level == IX_LEAF_LEVEL)
    {
        char *pTemp;
        PageNum tempNode;

        // 调整next leaf
        tempNode = ((IX_NodeHdr*)pThisData)->extraPtr;
        if(tempNode != IX_INVALID_NODE)
        {
            if((rc = pfFh.GetThisPage(tempNode, ph))  ||
               (rc = ph.GetData(pTemp)))
                return (rc);

            ((IX_NodeHdr*)pTemp)->prevPtr = ((IX_NodeHdr*)pThisData)->prevPtr;
            
            if((rc = pfFh.MarkDirty(tempNode))  ||
               (rc = pfFh.UnpinPage(tempNode)))
                return (rc);
        }

        // 调整 prev leaf
        tempNode = ((IX_NodeHdr*)pThisData)->prevPtr;
        if(tempNode != IX_INVALID_NODE)
        {
            if((rc = pfFh.GetThisPage(tempNode, ph))  ||
               (rc = ph.GetData(pTemp)))
                return (rc);

            ((IX_NodeHdr*)pTemp)->extraPtr = ((IX_NodeHdr*)pThisData)->extraPtr;
            
            if((rc = pfFh.MarkDirty(tempNode))  ||
               (rc = pfFh.UnpinPage(tempNode)))
                return (rc);
        }
        else// 若 thisNode 为第一个，则调整leafList
        {
            hdr.leafList = ((IX_NodeHdr*)pThisData)->extraPtr;
            bHdrChanged = TRUE;
        }
    }

    // 判断merge结束后，parent是否需要调整
    int minNum = (anchorNode == hdr.root) ? 1 : (hdr.keyNumPerPage / 2);
    if(((IX_NodeHdr*)pAnchorData)->keyNum > minNum)
        balanceNode = IX_INVALID_NODE;
    else if(balanceNode == IX_INVALID_NODE)
        balanceNode = anchorNode;

    // set dirty（除anchor）、unpin
    if((rc = pfFh.MarkDirty(thisNode))    || 
       (rc = pfFh.UnpinPage(thisNode))    ||
       (rc = pfFh.MarkDirty(neighborNode))||
       (rc = pfFh.UnpinPage(neighborNode))||
       (rc = pfFh.UnpinPage(anchorNode)))
       return (rc);

    done = thisNode;    // 交由上层释放thisNode

    return (OK_RC);
}

//
// PrintNode
//
// Desc: 打印当前node。局限于int类型。TODO支持更多类型
// In:   thisNode - 当前要打印 Node 的 PageNum
// Ret:  IX return code
//
RC IX_IndexHandle::PrintNode(PageNum thisNode, int spOff) const
{
    RC rc;
    PF_PageHandle ph;
    char *pData;
    int offset = sizeof(IX_NodeHdr);
    int attrLen = hdr.attrLength;
    int entryLen = attrLen + sizeof(PageNum);
    int i, j;

    if((rc = pfFh.GetThisPage(thisNode, ph))  ||
        (rc = ph.GetData(pData)))
        return (rc);

    // print IndexHdr
    if (spOff == hdr.height * 20)
        printf("\nheight=%-3d leaf=%-3d root=%-3d\n", hdr.height, hdr.leafList, hdr.root);

    // print hdr
    
    for(j = 0; j < spOff; ++j)   printf(" ");
    puts  ("------------------------------");
    for(j = 0; j < spOff; ++j)   printf(" ");
    printf  ("|%-3d  level=%-3d keyNum=%-3d   |\n", thisNode, ((IX_NodeHdr*)pData)->level, ((IX_NodeHdr*)pData)->keyNum);
    for(j = 0; j < spOff; ++j)   printf(" ");
    puts    ("|     ---    ----    ---     |");
    
    // print keys
    for(int i = 0; i < ((IX_NodeHdr*)pData)->keyNum; ++i)
    {    
        for(j = 0; j < spOff; ++j)   printf(" ");
        printf("|  key_%-3d=%5d  |  ptr=%-3d |\n", i, *(int*)(pData + offset), *(PageNum*)(pData + offset + attrLen));
        offset += entryLen;
    }
    
    for(j = 0; j < spOff; ++j)   printf(" ");
    puts    ("|     ---    ----    ---     |");
    for(j = 0; j < spOff; ++j)   printf(" ");
    printf  ("|    extra=%-3d  prev  =%-3d   |\n", ((IX_NodeHdr*)pData)->extraPtr, ((IX_NodeHdr*)pData)->prevPtr);
    for(j = 0; j < spOff; ++j)   printf(" ");
    puts    ("------------------------------");

    if(rc = pfFh.UnpinPage(thisNode))
        return (rc);

    return (OK_RC);
}

//
// PrintTree
//
// Desc: 打印整个Tree。局限于int类型。TODO支持更多类型
// Ret:  IX return code
//
RC IX_IndexHandle::PrintTree()  const
{
    RC rc;
    PF_PageHandle ph;
    std::vector<PageNum> child = { hdr.root };
    char *pData;
    int attrLen = hdr.attrLength;
    int entryLen = attrLen + sizeof(PageNum);
    int offset, numNode;
    int i, j, k;

    for(i = hdr.height; i > 0; --i)
    {
        std::vector<PageNum> temp;
        for(j = 0; j < child.size(); ++j)
        {
            if((rc = pfFh.GetThisPage(child[j], ph))    ||
               (rc = ph.GetData(pData)))
                return (rc);

            // ptint node
            PrintNode(child[j], ((IX_NodeHdr*)pData)->level * 20);

            if(i != IX_LEAF_LEVEL)
            {
                // 记录子树
                offset = sizeof(IX_NodeHdr);
                for(k = 0; k <= ((IX_NodeHdr*)pData)->keyNum; ++k)
                {
                    if(k == 0)
                        numNode = ((IX_NodeHdr*)pData)->extraPtr;
                    else
                    {    
                        numNode = *(PageNum*)(pData + offset + attrLen);
                        offset += entryLen;
                    }
                    temp.push_back(numNode);
                }
            }

            // unpin
            if(rc = pfFh.UnpinPage(child[j]))
                return (rc);
        }
        child = temp;
    }

    return (OK_RC);
}