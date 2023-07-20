//
// File:        rm_manager.cc
// Description: RM_Manager class implementation
// Authors:     Renzhong Wang (rzwang@mail.ustc.edu.cn)
//

#include <cstdio>
#include "rm_internal.h"

//
// RM_Manager
//
// Desc: Constructor - intended to be called once at begin of program
//       Handles creation, deletion, opening and closing of files.   TODO 实体类不用创建
//       It is associated with a PF_Manager that executes the file creation.
//
RM_Manager::RM_Manager(PF_Manager &pfm)
{
   pPfManager = &pfm;
}

//
// ~RM_Manager
//
// Desc: Destructor - intended to be called once at end of program
//       Destroys the buffer manager.     TODO 实体类不用手动析构
//       All files are expected to be closed when this method is called.
//
RM_Manager::~RM_Manager()
{
   pPfManager = NULL;
}

//
// CreateFile
//
// Desc: Create a new RM file named fileName
//       分配 page 0 并向其中存入RM文件头信息。
// In:   fileName - name of file to create
//       recordSize - Size of record in this file
// Ret:  RM return code
//
RC RM_Manager::CreateFile (const char *fileName, int recordSize)
{
   // 检查record是否跨页
   // 至少需要iB空间存储bitMap，故相等情况也不合法
   if(recordSize >= RM_PAGE_SIZE)
      return (RM_SIZEOUTOFPAGE);
   
   // 检查recordSize合法性
   if(recordSize <= 0)
      return (RM_SIZETOSMALL);

   RC rc;
   PF_FileHandle fh;
   PF_PageHandle ph;
   char *pData;
   PageNum hdrPageNum;
   int recNumPerPage = GetRecNumPerPage(recordSize);
   int bitmapSize = GetBitmapSize(recNumPerPage);

   // 校验计算结果
   if(recNumPerPage * recordSize + bitmapSize > RM_PAGE_SIZE)
      return (RM_BITMAPSIZEERR);

   if((rc = pPfManager->CreateFile(fileName))    ||
      (rc = pPfManager->OpenFile(fileName, fh))  ||
      (rc = fh.AllocatePage(ph))                 ||
      (rc = ph.GetData(pData)))
     return (rc);

   // 在 Page-0 中记录RM_Hdr信息
   *(RM_FileHdr*)pData = { recordSize,          // recordSize
                           recNumPerPage,       // recNumPerPage
                           0,                   // numPages
                           RM_PAGE_LIST_END,    // firstFree
                           sizeof(RM_PageHdr),  // bitmapOffset
                           bitmapSize        }; // bitmapSize
                                        
   if(rc = ph.GetPageNum(hdrPageNum))
      return (rc);

   if(hdrPageNum != 0)
      return (RM_ISNOTHDRPAGE);   
   
   if((rc = fh.UnpinPage(hdrPageNum))        ||
      (rc = pPfManager->CloseFile(fh)))
      return (rc);

   // Return ok
   return (OK_RC);
}

//
// DestroyFile
//
// Desc: Delete a RM file named fileName 
// In:   fileName - name of file to create
// Ret:  RM return code
//
RC RM_Manager::DestroyFile(const char *fileName)
{
   RC rc;
   if(rc = pPfManager->DestroyFile(fileName))
      return (rc);

   // Return ok
   return (OK_RC);
}

//
// OpenFile
//
// Desc: 打开页式文件"fileName"，并对传入的RM_FileHandle类型fileHandle变量
//       进行初始化。该过程中，RM文件头信息会被存入fileHandle。
// In:   fileName - name of file to create
// Out:  fileHandle - TODO
// Ret:  RM return code
//
RC RM_Manager::OpenFile(const char *fileName, RM_FileHandle &fileHandle)
{
   RC rc;
   PF_PageHandle ph;
   PageNum hdrPageNum;
   char* pData;
   
   // 文件未打开
   if (fileHandle.bFileOpen)
      return (RM_OPENEDFILE);

   // 设置Handle中PF_FileHandle对象指向打开文件
   if((rc = pPfManager->OpenFile(fileName, fileHandle.pfFh)))
      return (rc);

   // 读取头部信息            // TODO 等fileHande类写好在来检查一次
   if((rc = fileHandle.pfFh.GetFirstPage(ph))   ||
      (rc = ph.GetData(pData))                  ||
      (rc = ph.GetPageNum(hdrPageNum)))
      return (rc);

   // 检查hdr对应页号合法性
   if(hdrPageNum)
      return (RM_ISNOTHDRPAGE);

   // 写入RM文件头信息
   fileHandle.hdr = *(RM_FileHdr*)pData;
   
   // unpinned
   if(rc = fileHandle.pfFh.UnpinPage(hdrPageNum))
      return (rc);

   // 未改变头部
   fileHandle.bHdrChanged = FALSE;

   // RM_FileHandle设为打开
   fileHandle.bFileOpen = TRUE;

   // Return ok
   return (OK_RC);
}

// Desc: 关闭该RM类型文件，关闭过程中先将相关page（含Hdr）写回磁盘，然后关闭。
// In:   fileHandle - handle of file to close
// Out:  fileHandle - no longer refers to an open file
//                    this function modifies local var's in fileHandle
// Ret:  RM return code
//
RC RM_Manager::CloseFile(RM_FileHandle &fileHandle)
{
   RC rc;

   // Ensure fileHandle refers to open file
   if (!fileHandle.bFileOpen)
      return (RM_CLOSEDFILE);

   // Buffer Pool中与本文件相关的Pages全部落盘，并写回header信息
   if(rc = fileHandle.ForcePages())
      return (rc);

   // 关闭PF_FileHandle成员变量
   if(rc = pPfManager->CloseFile(fileHandle.pfFh))
      return (rc);

   fileHandle.bFileOpen = FALSE;

   // Return ok
   return (OK_RC);
}

//
// ForcePages
//
// Desc: Internal. 根据record size 计算并返回每个page上容纳的record个数
// In:   
// Ret:  
//
int RM_Manager::GetRecNumPerPage(const int recordSize) const
{
   // 计算一个 page 上能承载的 record 数量
   int recNum = (RM_PAGE_SIZE * 8) / (8 * recordSize + 1);


   // 若计算出的recNum偏小，则+1进行调整
   int tempRecNum = recNum +1;
   while( tempRecNum / 8 + !!(tempRecNum % 8) + tempRecNum * recordSize <= RM_PAGE_SIZE )
   {
      recNum = tempRecNum;
      tempRecNum++;
   }

   return recNum;
}

//
// ForcePages
//
// Desc: Internal. 根据每个Page上容纳的record个数计算bitmap size
// In:   
// Ret:  
//
int RM_Manager::GetBitmapSize(const int recNumPerPage) const
{
   int bitmapSize = recNumPerPage / 8;

   // 向上取整
   if(recNumPerPage % 8)
      bitmapSize++;

   return bitmapSize;
}