//
// rm.h
//
//   Record Manager component interface
//
// This file does not include the interface for the RID class.  This is
// found in rm_rid.h
//

#ifndef RM_H
#define RM_H

// Please DO NOT include any files other than redbase.h and pf.h in this
// file.  When you submit your code, the test program will be compiled
// with your rm.h and your redbase.h, along with the standard pf.h that
// was given to you.  Your rm.h, your redbase.h, and the standard pf.h
// should therefore be self-contained (i.e., should not depend upon
// declarations in any other file).

// Do not change the following includes
#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"

//
// RM_FileHeader: Header for each file
//
struct RM_FileHdr {
  int recordSize;       // record size in file

  int recNumPerPage;    // calculated max # of recs per page
                        // same as bitmap size

  PageNum numPages;     // number of pages
  PageNum firstFree;    // pointer to first free object

  int bitmapOffset;     // location in bytes of where the bitmap starts
                        // in the page headers
  int bitmapSize;       // size of bitmap (in Bytes, same as 'char')
};

//
// RM_Record: RM Record interface
//
class RM_Record {
    friend class RM_FileHandle;     // FileHdl 可访问 Record中私有变量
    // friend class RM_FileScan;    
    // friend class QL_Manager;     // TODO 这仨有啥作用吗，反而是加强了耦合性
public:
    RM_Record ();
    ~RM_Record();

    // Overloaded =
    RM_Record& operator=(const RM_Record &rec);

    // Return the data corresponding to the record.  Sets *pData to the
    // record contents.
    RC GetData(char *&pData) const;

    // Return the RID associated with the record
    RC GetRid (RID &rid) const;
private:
    RID rid;
    char *pData;
    int recordSize;
};

//
// RM_FileHandle: RM File interface
//
class RM_FileHandle {
    friend class RM_Manager;                            // RM_Mgr可管理文件Hdl
    friend class RM_FileScan;                           // RM_Scan可管理文件Hdl
public:
    RM_FileHandle ();
    ~RM_FileHandle();

    // Given a RID, return the record
    RC GetRec     (const RID &rid, RM_Record &rec) const;

    RC InsertRec  (const char *pData, RID &rid);       // Insert a new record

    RC DeleteRec  (const RID &rid);                    // Delete a record
    RC UpdateRec  (const RM_Record &rec);              // Update a record

    // Forces a page (along with any contents stored in this class)
    // from the buffer pool to disk.  Default value forces all pages.
    RC ForcePages (PageNum pageNum = ALL_PAGES) const;

private:
    RM_FileHdr hdr;                                             // file header
    PF_FileHandle pfFh;                                        // pf page handle
    bool bFileOpen;                                              // file open flag
    bool bHdrChanged;                                            // dirty flag for file hdr
    
    // Functions for handling bitmap
    bool GetBit         (const char *pBitmap, SlotNum slotNum) const;
    void SetBit         (char *pBitmap, SlotNum slotNum) const;
    void UnsetBit       (char *pBitmap, SlotNum slotNum) const;
    SlotNum FindFreeSlot(const char *pBitmap) const;
};

//
// RM_Manager: provides RM file management
//
class RM_Manager {
    // friend class QL_Manager;        // for accessing pf manager TODO
public:
    RM_Manager    (PF_Manager &pfm);
    ~RM_Manager   ();

    RC CreateFile (const char *fileName, int recordSize);
    RC DestroyFile(const char *fileName);
    RC OpenFile   (const char *fileName, RM_FileHandle &fileHandle);

    RC CloseFile  (RM_FileHandle &fileHandle);
    
private:
    PF_Manager *pPfManager;
    
    // 根据recordSize计算每个Page上容纳的record个数     TODO，测试是否需要加入调整bitmapSize的部分。
    int GetRecNumPerPage(const int recordSize) const;
    
    // 根据每个Page上容纳的record个数计算bitmap size
    int GetBitmapSize(const int recNumPerPage) const;
};

//
// RM_FileScan： RM File Scan interface
//
class RM_FileScan {
public:
    RM_FileScan  ();
    ~RM_FileScan ();

    RC OpenScan  (const RM_FileHandle &fileHandle,
                  AttrType   attrType,
                  int        attrLength,
                  int        attrOffset,
                  CompOp     compOp,
                  void       *value,
                  ClientHint pinHint = NO_HINT); // Initialize a file scan
    RC GetNextRec(RM_Record &rec);               // Get next matching record
    RC CloseScan ();                             // Close the scan

private:

    bool bScanOpen;

    RM_FileHandle *pRmFh;
    int      attrLength;
    int      attrOffset;
    AttrType attrType;
    bool    (*Operate)(void *pValue1, void *pValue2, AttrType attrType, int attrLength);
    void    *pValue;
    ClientHint pinHint = NO_HINT; 

    // 记录当前遍历位置
    PageNum currentPage;
    SlotNum currentSlot;

    SlotNum GetNextRecSlot(const char *pBitmap) const;
};

//
// Print-error function
//
void RM_PrintError(RC rc);

// Warnings
#define RM_SIZEOUTOFPAGE            (START_RM_WARN + 0)     // Record size is too large
#define RM_SIZETOSMALL              (START_RM_WARN + 1)     // Record size is too small
#define RM_ISNOTHDRPAGE             (START_RM_WARN + 2)     // RM_File Hdr 没有存储在 page 0 上
#define RM_OPENEDFILE               (START_RM_WARN + 3)     // 文件已经打开
#define RM_CLOSEDFILE               (START_RM_WARN + 4)     // 文件已经关闭
#define RM_INVALIDPAGENUM           (START_RM_WARN + 5)     // PageNum 错误 
#define RM_INVALIDSLOTNUM           (START_RM_WARN + 6)     // SlotNum 错误
#define RM_INVALIDRECORD            (START_RM_WARN + 7)     // Record 未初始化
#define RM_OPENEDSCAN               (START_RM_WARN + 8)     // Scan 已经打开
#define RM_CLOSEDSCAN               (START_RM_WARN + 9)     // Scan 已经关闭
#define RM_UNDEFATTRTYPE            (START_RM_WARN + 10)    // 未定义的Type
#define RM_INVALIDATTRLEN           (START_RM_WARN + 11)    // 属性长度错误
#define RM_INVALIDATTROFFSET        (START_RM_WARN + 12)    // 属性值offset错误
#define RM_UNDEFCOMPOP              (START_RM_WARN + 13)    // 未定义的运算符
#define RM_EOF                      (START_RM_WARN + 14)    // End of file
#define RM_LASTWARN                 RM_EOF

// Errors
#define RM_INVALIDRECORDNUM         (START_RM_ERR - 0) // Invalid PC recdor name
#define RM_INVALIDNAME              (START_RM_ERR - 1) // Invalid PC file name
#define RM_RECSCANERR               (START_RM_ERR - 2) // Scan 运行错误
#define RM_BITMAPSIZEERR            (START_RM_ERR - 3) // Inconsistent bitmap in page

// Error in UNIX system call or library routine
#define RM_UNIX                     (START_RM_ERR - 4) // Unix error
#define RM_LASTERROR                RM_UNIX

#endif