//
// File:        rm_filehandle.cc
// Description: RM_FileHandle class implementation
// Authors:     Renzhong Wang (rzwang@mail.ustc.edu.cn)
//

#include "rm_internal.h"

//
// 
//
// Desc: 
// In:   
// Ret:  
//
RM_FileHandle::RM_FileHandle()
{
    bFileOpen = FALSE;
}

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RM_FileHandle::~RM_FileHandle()
{
    // Don't need to do anything
}

//
// 
//
// Desc: 根据RID获取对应的Record，对Record内容进行 **拷贝** 后，
//       将该拷贝出的Record副本的地址赋给rec，再把rid也存入rec后作为返回值。
// In:   
// Ret:  
//
RC RM_FileHandle::GetRec(const RID &rid, RM_Record &rec) const
{
    // File must be open
    if (!bFileOpen)
      return (RM_CLOSEDFILE);

    RC rc;
    PF_PageHandle ph;
    PageNum pageNum;
    SlotNum slotNum;
    RM_PageHdr *pPageHdr;
    char *pData;

    // 打开对应page，并读出数据
    if((rc = rid.GetPageNum(pageNum))           ||
       (rc = pfFh.GetThisPage(pageNum, ph))     ||
       (rc = ph.GetData(pData)))
        return (rc);     
       
    // 检查page合法性
    pPageHdr = (RM_PageHdr*)pData;
    if(pPageHdr->recordNum == 0)
        return (RM_INVALIDPAGENUM); 

    // 获取此page上待获取rec的slotNum
    if(rc = rid.GetSlotNum(slotNum))
        return (rc);

    // 检查slot合法性
    // 注：pData已经跳过PF_PageHdr部分了
    int offset = hdr.bitmapOffset;
    if((FALSE == GetBit(pData + offset, slotNum))     ||
       (slotNum >= hdr.recNumPerPage))
        return (RM_INVALIDSLOTNUM);

    // 根据slotNum计算Rec内容的起始地址     TODO 多次计算的话，就函数化。
    offset = hdr.bitmapSize + hdr.recordSize * slotNum;
        
    // 将Record内容拷贝后，把地址赋给rec中指针
    rec.recordSize = hdr.recordSize;
    rec.pData = new char[hdr.recordSize];
    strncpy(rec.pData, pData + offset, hdr.recordSize);     // 注：strncpy不加字符串终止符

    // 将rid存入rec
    rec.rid = rid;

    // unpinned page
    rc = pfFh.UnpinPage(pageNum);
    
    return (OK_RC);
}

//
// InsertRec
//
// Desc: 从当前RM_File的freeList中找到一个空位，插入数据; 若freeList中没有空位，
//       则分配一个新的Page用于插入数据。数据插入后更新RM_FileHdr（如freeList），
//       最后返回其RID。
// 
//       TODO 头部加入有序标记后，实现根据字段值的有序插入
// In:   
// Ret:  
//
RC RM_FileHandle::InsertRec(const char *pData, RID &rid)
{
    // File must be open
    if (!bFileOpen)
      return (RM_CLOSEDFILE);



    // 局部变量
    RC rc;
    RM_PageHdr *pPageHdr;
    PF_PageHandle ph;
    PageNum pageNum;
    SlotNum slotNum;
    char *pPageData;
    int offset;



    // 找到一个有空位的page，将pPageData指向page内容
    if(hdr.firstFree != RM_PAGE_LIST_END)
    {   // freeList中有未满的page...
        pageNum = hdr.firstFree;   
        if((rc = pfFh.GetThisPage(pageNum, ph))     ||
           (rc = ph.  GetData    (pPageData)))
            return (rc);
    }
    else
    {   // freeList为空，需要分配新的page...

        // 分配新page
        if((rc = pfFh.AllocatePage(ph))             ||
           (rc = ph.  GetPageNum  (pageNum)))
            return (rc);

        // 初始化新page的RM_PageHdr
        if(rc = ph.GetData(pPageData))
            return (rc);
        
        pPageHdr = (RM_PageHdr*)pPageData;
        pPageHdr->nextFree = RM_PAGE_LIST_END;
        pPageHdr->recordNum = 0;

        // 初始化bitmap
        memset(pPageData + hdr.bitmapOffset, 0, hdr.bitmapSize);
    }



    // 找到bitmap中空闲slot
    slotNum = FindFreeSlot(pPageData + hdr.bitmapOffset);
    if(slotNum >= hdr.recNumPerPage)
        return (RM_INVALIDSLOTNUM);

    // 插入数据并获取RID
    offset = hdr.bitmapOffset + hdr.bitmapSize + hdr.recordSize * slotNum;
    strncpy(pPageData + offset, pData, hdr.recordSize);
    RID temp(pageNum, slotNum);
    rid = temp;

    // 更新RM_PageHdr中recordNum
    pPageHdr = (RM_PageHdr*)pPageData;
    pPageHdr->recordNum++;

    // 检查record num合法性
    if(pPageHdr->recordNum > hdr.recNumPerPage)
        return (RM_INVALIDRECORDNUM);

    // 更新bitmap
    SetBit(pPageData + hdr.bitmapOffset, slotNum);



    // 调整freeList
    if(hdr.firstFree != RM_PAGE_LIST_END)
    {   // 该page从freelist中获得...
        
        // 插入后page满了，则移出freeList
        if (pPageHdr->recordNum == hdr.recNumPerPage)
        {
            hdr.firstFree = pPageHdr->nextFree;
            pPageHdr->nextFree = RM_PAGE_LIST_END;      // TODO 根据后续需要，可改设为已满标识，用于快速判断Page是否已满
            
            // 文件头改动
            bHdrChanged = TRUE;
        }
    }
    else
    {
        // 该page是新分配的...

        // 未满则插入freeList
        if (pPageHdr->recordNum != hdr.recNumPerPage)
        {
            pPageHdr->nextFree = hdr.firstFree;
            hdr.firstFree = pageNum;

            // 文件头改动
            bHdrChanged = TRUE;
        }
    }



    // set dirty bit
    pfFh.MarkDirty(pageNum);
    
    // unpinned page
    pfFh.UnpinPage(pageNum);

    return (OK_RC);
}

//
// DeleteRec
//
// Desc: 根据RID获取对应page，并删除其中slotNum位置上的record
//       最后RM_PageHdr和RM_PageHdr中的freeList。
//
//       TODO 有序状态下的删除，要加入顺序调整操作
// In:   
// Ret:  
//
RC RM_FileHandle::DeleteRec(const RID &rid)
{
    // File must be open
    if (!bFileOpen)
      return (RM_CLOSEDFILE);


    // 局部变量
    RC rc;
    PF_PageHandle ph;
    PageNum pageNum;
    SlotNum slotNum;
    RM_PageHdr *pPageHdr;
    char *pData;



    // 读取RID
    if((rc = rid.GetPageNum(pageNum))       ||
       (rc = rid.GetSlotNum(slotNum)))
        return (rc);

    // 打开文件，获取指向page内容的指针
    if((rc = pfFh.GetThisPage(pageNum, ph)) ||
       (rc = ph.GetData(pData)))
        return (rc);

    // 检查page内record数目合法性
    pPageHdr = (RM_PageHdr*)pData;
    if(pPageHdr->recordNum <= 0)
       return (RM_INVALIDRECORDNUM);

    // 更改文件头实现删除record
    UnsetBit(pData + hdr.bitmapOffset, slotNum);
    pPageHdr->recordNum--;
        
    // 若page由满转为有空余状态，调整freelist
    if((pPageHdr->recordNum + 1) == hdr.recNumPerPage)
    {
        pPageHdr->nextFree = hdr.firstFree;
        hdr.firstFree = pageNum;

        // 文件头改动
        bHdrChanged = TRUE;
    }



    // set dirty bit
    pfFh.MarkDirty(pageNum);
    
    // unpinned page
    pfFh.UnpinPage(pageNum);

    return (OK_RC);
}

//
// 
//
// Desc: 根据RID获取对应page，更新其中slotNum位置上的record
//       RM_PageHdr和RM_PageHdr都不变化。
//
//       TODO 有序状态下的更新，可以通过调用删除和插入实现
// In:   
// Ret:  
//
RC RM_FileHandle::UpdateRec(const RM_Record &rec)
{
    // File must be open
    if (!bFileOpen)
      return (RM_CLOSEDFILE);


    
    // 局部变量
    RC rc;
    PF_PageHandle ph;
    PageNum pageNum;
    SlotNum slotNum;
    char *pData;



    // 读取RID
    if((rc = rec.rid.GetPageNum(pageNum))       ||
       (rc = rec.rid.GetSlotNum(slotNum)))
        return (rc);

    // 打开文件，获取指向page内容的指针
    if((rc = pfFh.GetThisPage(pageNum, ph))     ||
       (rc = ph.  GetData    (pData)))
        return (rc);

    // 更新文件中相应记录
    int offset = hdr.bitmapOffset + hdr.bitmapSize + hdr.recordSize * slotNum;
    strncpy(pData + offset, rec.pData, hdr.recordSize);




    // set dirty bit
    pfFh.MarkDirty(pageNum);
    
    // unpinned page
    pfFh.UnpinPage(pageNum);

    return (OK_RC);
}

//
// ForcePages
//
// Desc: 将RM类型文件相关的 page 写回磁盘。
//       同时如果 RM_FileHdr 发生变动也一起写回磁盘。
// In:   
// Ret:  
//
RC RM_FileHandle::ForcePages(PageNum pageNum) const
{
    RC rc;
    PF_PageHandle ph;
    char *pData;
    PageNum hdrPageNum;

    // File must be open
    if (!bFileOpen)
      return (RM_CLOSEDFILE);

    // 如果文件头被改动，将其写回文件
    if(bHdrChanged)
    {
        // 读入hdr对应page
        if((rc = pfFh.GetFirstPage(ph))         ||
           (rc = ph.GetPageNum(hdrPageNum)))
            return (rc);

        // 检查存储hdr的page合法性
        if(hdrPageNum)
            return (RM_ISNOTHDRPAGE);

        // 更新hdr信息
        if(rc = ph.GetData(pData))
            return (rc);

        *(RM_FileHdr*)pData = this->hdr;

        // set dirty, unpinned
        if((rc = pfFh.MarkDirty(hdrPageNum))    ||
           (rc = pfFh.UnpinPage(hdrPageNum)))
            return (rc);

        // hdr信息落盘
        if(rc = pfFh.ForcePages(hdrPageNum))
            return (rc);

        // This function is declared const, but we need to change the
        // bHdrChanged variable.  Cast away the constness
        RM_FileHandle *dummy = (RM_FileHandle *)this;
        dummy->bHdrChanged = FALSE;
    }

    // 调用PF层函数写回指定page
    return (pfFh.ForcePages(pageNum));
}

//
// GetBit
//
// Desc: 根据slotNum找到bitmap中对应位置的bit值并返回。
// In:   
// Ret:  
//
bool RM_FileHandle::GetBit(const char *pBitmap, SlotNum slotNum) const
{
    // 获取bitmap中位置
    int i = slotNum / 8;
    int j = slotNum % 8;

    char temp = pBitmap[i];
    char mask = 1<<j;

    if (temp & mask)
        return TRUE;
    else
        return FALSE;
}

//
// SetBit
//
// Desc: 根据slotNum找到bitmap中对应位置的bit值并返回。
// In:   
// Ret:  
//
void RM_FileHandle::SetBit(char *pBitmap, SlotNum slotNum) const
{
    // 获取bitmap中位置
    int i = slotNum / 8;
    int j = slotNum % 8;

    char mask = 0x80 >> j;
    pBitmap[i] |= mask;
}

//
// UnsetBit
//
// Desc: 根据slotNum找到bitmap中对应位置的bit值并返回。
// In:   
// Ret:  
//
void RM_FileHandle::UnsetBit(char *pBitmap, SlotNum slotNum) const
{
    // 获取bitmap中位置
    int i = slotNum / 8;
    int j = slotNum % 8;

    char mask = ~(0x80 >> j);
    pBitmap[i] |= mask;
}

//
// FindFreeSlot
//
// Desc: 通过查找bitmap中第一个0-bit来找到page中空余的slot。
//       查找运用了循环展开，展开大小为4B（sizeof(int) = 32bit）。
//       默认调用前已检查过page是否还有空间。
// In:   
// Ret:  
//
SlotNum RM_FileHandle::FindFreeSlot(const char *pBitmap) const
{
    unsigned int *bitArray = (unsigned int *)pBitmap;
    int limit = hdr.bitmapSize / 4;
    
    int i, j;
    char temp, mask;

    // 按4B查找
    for(i = 0;i < limit;++i)
        // 当前4B中存在空位
        if(bitArray[i] != 0xFFFFFFFFu)
            break;
    // endfor

    // 按1B查找
    for(i*=4;i<hdr.bitmapSize - 1;i++)
    {
        if(pBitmap[i] != 0xFFu)
        {   // 当前1B中存在空位
            temp = pBitmap[i];
            mask = 0x80;                        // 1000 0000

            for(j = 0; j < 8; ++j, mask >>= 1)
                if ((mask & temp) == 0u)
                    return (i*8+j);
            // endfor
        }
    }

    // 最后一字节中bit位不一定全部可用，需要特别处理
    limit = hdr.recNumPerPage % 8;
    temp = pBitmap[i];
    mask = 0x80;
    for(j = 0; j < limit; ++j, mask >>= 1)
        if ((mask & temp) == 0u)
            return (i*8+j);
    // endfor

    // 未找到slot
    return (hdr.recNumPerPage);
}





