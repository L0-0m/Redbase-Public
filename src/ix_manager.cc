//
// File:        ix_manager.cc
// Description: IX_Manager class implementation
// Authors:     L0-0m (rzwang@mail.ustc.edu.cn)
//

#include <string>
#include "ix_internal.h"

//
// IX_Manager
//
// Desc: Constructor
//
IX_Manager::IX_Manager   (PF_Manager &pfm)
{
    pPfManager = &pfm;
}

//
// ~IX_Manager
//
// Desc: Destructor
//
IX_Manager::~IX_Manager  ()
{
    pPfManager = NULL;
}

//
// CreateIndex
//
// Desc: Create a new IX file named "fileName.indexNo"
//       分配 page 0 并向其中存入IX文件头信息。
// In:   fileName - name of file to create
// Ret:  IX return code
//
RC IX_Manager::CreateIndex  (const char *fileName,
                              int        _indexNo,
                              AttrType   _attrType,
                              int        _attrLength)
{
    // 进行参数检查
	if((_attrType < INT)                            ||
	   (_attrType > STRING))
		return (IX_UNDEFATTRTYPE);

    // TODO 禁用 NO_EQ
	if((_attrType == INT)    && (_attrLength != 4)  ||
	   (_attrType == STRING) && (_attrLength > MAXSTRINGLEN))        // TODO 支持新类型时需要改动
		return (IX_INVALIDATTRLEN);

    if(_indexNo < 0)
        return (IX_INVALIDINDEXNO);

    RC rc;
    PF_FileHandle fh;
    PF_PageHandle ph;
    char *pData;
    PageNum hdrPageNum;
    
    // 构造 index 文件名
    std::string indexFileName = GetIndexFileName(fileName, _indexNo);
    
    // 创建文件，并返回 page 0
    if((rc = pPfManager->CreateFile(indexFileName.c_str()))     ||
       (rc = pPfManager->OpenFile(indexFileName.c_str(), fh))   ||
       (rc = fh.AllocatePage(ph))                       ||
       (rc = ph.GetData(pData)))
        return (rc);

    // 计算每个page容纳key-pointer对个数
    int keyNumPerPage = GetKeyNumPerPage(_attrLength);
    if(keyNumPerPage < 1)
        return (IX_INVALIDKEYNUM);

    // 计算每个page容纳key-pointer对个数
    int ridNumPerPage = GetRidNumPerPage(_attrLength);
    if(ridNumPerPage < 1)
        return (IX_INVALIDRIDNUM);

#ifdef IX_DEBUG

    printf("\nkeyNum=%5d ridNum=%5d", keyNumPerPage, ridNumPerPage);

#ifdef IX_NODE_DEGREE
    keyNumPerPage = IX_NODE_DEGREE;
#endif

#ifdef IX_BUCKET_DEGREE
    ridNumPerPage = IX_BUCKET_DEGREE;
#endif
    printf("\nkeyNum=%5d ridNum=%5d\n", keyNumPerPage, ridNumPerPage);

#endif

    // 在 page 0 中记录IX_Hdr信息
   *(IX_IndexHdr*)pData = { _indexNo,           // indexNo
                           _attrType,           // attrType
                           _attrLength,         // attrLength
                           0,                   // height of B+tree
                           keyNumPerPage,       // keyNumPerPage
                           ridNumPerPage,       // ridNumPerPage
                           IX_INVALID_NODE,     // root pageNum
                           IX_INVALID_NODE };   // leafList

    // 获取 IX Hdr 存储的位置
    if(rc = ph.GetPageNum(hdrPageNum))
      return (rc);

    if(hdrPageNum != 0)
      return (IX_ISNOTHDRPAGE);   
   
    // unpinned & close
    if((rc = fh.UnpinPage(hdrPageNum))        ||
      (rc = pPfManager->CloseFile(fh)))
      return (rc);

    // Return ok
    return (OK_RC);
}                     

//
// DestroyIndex
//
// Desc: 删除索引文件
// In:   
// Ret:
//
RC IX_Manager::DestroyIndex (const char *fileName, int _indexNo)
{
    RC rc;
    // 构造 index 文件名
    std::string indexFileName = GetIndexFileName(fileName, _indexNo);

    if(rc = pPfManager->DestroyFile(indexFileName.c_str()))
        return (rc);

    // Return ok
    return (OK_RC);
}

//
// OpenIndex
//
// Desc: 打开文件"fileName.indexNo"，并初始化indexHandle（将 page 0 中
//       IX Index Hdr信息读入内存）。最后保持B+树根节点常驻内存。
// In:   
// Out:
// Ret:
//
RC IX_Manager::OpenIndex(const char     *fileName,
                         int            _indexNo,
                         IX_IndexHandle &indexHandle)
{
    // 参数检查
    if(_indexNo < 0)
        return (IX_INVALIDINDEXNO);

    // 检查文件是否打开
    if(indexHandle.bIndexOpen == TRUE)
        return (IX_OPENEDFILE);

    RC rc;
    PF_PageHandle ph;
    PageNum hdrPageNum;
    char *pData;

    // 构造 index 文件名
    std::string indexFileName = GetIndexFileName(fileName, _indexNo);

    // 打开文件
    if(rc = pPfManager->OpenFile(indexFileName.c_str(), indexHandle.pfFh))
        return (rc);    

    // 读取头部信息
    if((rc = indexHandle.pfFh.GetFirstPage(ph))     ||
       (rc = ph.GetData(pData))                     ||
       (rc = ph.GetPageNum(hdrPageNum)))
        return (rc);

    // 检查hdr对应页号合法性
    if(hdrPageNum != 0)
        return (IX_ISNOTHDRPAGE);
    
    // 写入IX文件头信息
    indexHandle.hdr = *(IX_IndexHdr*)pData;

    // unpinned hdr page
    if(rc = indexHandle.pfFh.UnpinPage(hdrPageNum))
        return (rc);

    // 设IndexHandle为打开
    indexHandle.bIndexOpen = TRUE;

    // 未改变IndexFileHdr
    indexHandle.bHdrChanged = FALSE;

    return (OK_RC);
}

//
// CloseIndex
//
// Desc: 将IX类型索引文件相关的 page 全部写回磁盘。
//       同时如果 IX_indexHdr 发生变动也一起写回磁盘。
// In:   
// Out:
// Ret:
//
RC IX_Manager::CloseIndex(IX_IndexHandle &indexHandle)
{
    RC rc;

    // Ensure indexHandle refers to open file
    if (!indexHandle.bIndexOpen)
        return (IX_CLOSEDINDEX);

    // Buffer Pool中与该索引相关的 Page 全部落盘，并写回header信息（参数省略即为 ALL PAGES）
    if(rc = indexHandle.ForcePages())
        return (rc);

    // 关闭 PF_FileHandle 成员变量
    if(rc = pPfManager->CloseFile(indexHandle.pfFh))
        return (rc);

    // 关闭文件
    indexHandle.bIndexOpen = FALSE;

    // Return ok
    return (OK_RC);
}

//
// GetIndexFileName
//
// Desc: 根据 fielName 和 indexNo 构建形如 "fileName.index" 的索引文件名
// In:   
// Out:
// Ret:
//
std::string IX_Manager::GetIndexFileName(const char *fileName, int indexNo)
{
    std::string delimiter = ".";
    std::string indexFileName = std::string(fileName) + delimiter + std::to_string(indexNo);

    return indexFileName;
}

//
// GetKeyNumPerPage
//
// Desc: 计算Node中容纳的key-pointer个数
// In:   
// Out:
// Ret:
//
int IX_Manager::GetKeyNumPerPage(int _attrLength) const
{
    return IX_NODE_SIZE / (_attrLength + sizeof(PageNum));
}

//
// GetRidNumPerPage
//
// Desc: 计算Buceket中容纳的rid个数
// In:   
// Out:
// Ret:
//
int IX_Manager::GetRidNumPerPage(int _attrLength) const
{
    // bucket中每个元素存放 rid 和 一个next 指针
    // TODO 考虑使用内存操作
    // return IX_BUCKET_SIZE / (sizeof(RID) + sizeof(short));
    return IX_BUCKET_SIZE / (sizeof(IX_RidEntry));
}