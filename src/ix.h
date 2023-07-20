//
// ix.h
//
//   Index Manager Component Interface
//

#ifndef IX_H
#define IX_H

// Please do not include any other files than the ones below in this file.

#include "redbase.h"
#include "rm_rid.h"
#include "pf.h"
#include "rm.h"
#include <string>

//
// Debug
//
#define IX_DEBUG
#define IX_NODE_DEGREE     3        // node中最大entry个数
#define IX_BUCKET_DEGREE   3        // bucket中最大rid个数

//
// IX_FileHeader: Header for each file
//
struct IX_IndexHdr {
    int indexNo;
    AttrType attrType;
    int      attrLength;
    int height;             // 当前 B+Tree 层数，不包含 bucket 层
    int keyNumPerPage;      // 每个 page 存放 key-pointer对 的个数（不含extra pointer）
    int ridNumPerPage;      // 每个 bucket 存放 rid 的个数
    PageNum root;           // B+Tree 根存节点放位置
    PageNum leafList;       // leaf page 起始
};

//
// IX_IndexHandle: IX Index File interface
//
class IX_IndexHandle {
    friend class IX_Manager;
    friend class IX_IndexScan;

public:
    IX_IndexHandle  ();                             // Constructor
    ~IX_IndexHandle ();                             // Destructor
    RC InsertEntry  (void *pData, const RID &rid);  // Insert new index entry
    RC DeleteEntry  (void *pData, const RID &rid);  // Delete index entry
    RC ForcePages   ();                             // Copy index to disk
    // RC Traverse(int nodeNum = 0, int depth = 0);

    RC PrintNode(PageNum pageNum, int offset = 0) const;
    RC PrintTree() const;
private:

    IX_IndexHdr hdr;
    PF_FileHandle pfFh;
    char* pRootData;
    bool bIndexOpen;
    bool bHdrChanged;
    PageNum balanceNode;
    char* pLeftAnchorKey;
    PageNum leftAnchor;

    // char* debugPtr;

    // 搜索相关
    RC BinarySearch(void *key, const PageNum thisNode, int &pos, PageNum &childNode) const;
    RC FindRid            (int &pos, PageNum &thisNode, const RID &rid);

    // 插入相关
    RC SplitNode (void *&key, PageNum &childNode, PageNum thisNode,   int pos);
    RC InsertNode(void  *key, PageNum  thisNode,  PageNum childNode , int pos);
    RC InsertKey (void *&key, PageNum &childNode, const RID &rid);
    RC InsertBucket          (PageNum  thisNode,  const RID &rid);
    RC CreateNode            (PageNum &newNode,   int level);   // 创建一个新node
    RC CreateBucket          (PageNum &newBucket);              // 创建一个新bucket

    // 删除相关
    RC FindRebalance(PageNum &root,     PageNum rootNode,    PageNum leftNode, 
                     PageNum rightNode, PageNum lAnchor,     PageNum rAnchor, void *key, const RID &rid);
    RC DeleteBucket (PageNum &thisNode, const RID &rid);
    RC CollapseRoot (PageNum &newRoot,  PageNum thisNode);
    RC Rebalance    (PageNum &done,     PageNum thisNode,    PageNum leftNode,
                     PageNum rightNode, PageNum lAnchor,     PageNum rAnchor);
    RC Shift        (PageNum &done,     PageNum thisNode,    PageNum neighborNode, PageNum anchorNode);
    RC Merge        (PageNum &done,     PageNum thisNode,    PageNum neighborNode, PageNum anchorNode);
};

//
// IX_Manager: provides IX index file management
//
class IX_Manager {
public:
    IX_Manager      (PF_Manager &pfm);              // Constructor
    ~IX_Manager     ();                             // Destructor
    RC CreateIndex  (const char *fileName,          // Create new index
                     int        indexNo,
                     AttrType   attrType,
                     int        attrLength);
    RC DestroyIndex (const char *fileName,          // Destroy index
                     int        indexNo);
    RC OpenIndex    (const char *fileName,          // Open index
                     int        indexNo,
                     IX_IndexHandle &indexHandle);
    RC CloseIndex   (IX_IndexHandle &indexHandle);  // Close index

private:
    PF_Manager *pPfManager;

    std::string GetIndexFileName(const char *fileName, int indexNo);
    int GetKeyNumPerPage(int _attrLenght) const;
    int GetRidNumPerPage(int _attrLenght) const;
};

//
// IX_IndexScan: condition-based scan of index entries
//
class IX_IndexScan {
public:
    IX_IndexScan  ();                                 // Constructor
    ~IX_IndexScan ();                                 // Destructor
    RC OpenScan      (const IX_IndexHandle &indexHandle, // Initialize index scan
                      CompOp      compOp,
                      void        *value,
                      ClientHint  pinHint = NO_HINT);
    RC GetNextEntry  (RID &rid);                         // Get next matching entry
    RC CloseScan     ();                                 // Terminate index scan

private:
    IX_IndexHandle* pIxIh;
    bool bScanOpen;
    bool bNext;
    char *pValue;
    bool (*Operate)(void *pValue1, void *pValue2, AttrType attrType, int attrLength);

    // 记录当前遍历位置
    int currentNode;
    int currentEntryPos;
    int currentBucket;
    int currentRidPos;

    RC FindLeaf(PageNum &thisNode);
    RC GetNextPos();
};

//
// Print-error function and IX return code defines
//
void IX_PrintError(RC rc);

#define IX_UNDEFATTRTYPE        (START_IX_WARN + 0)     // 索引类型错误
#define IX_INVALIDATTRLEN       (START_IX_WARN + 1)     // 类型长度错误
#define IX_INVALIDINDEXNO       (START_IX_WARN + 2)     // 无效的索引号
#define IX_ISNOTHDRPAGE         (START_IX_WARN + 3)     // Index File Hdr 位置错误
#define IX_OPENEDFILE           (START_IX_WARN + 4)     // 打开一个已经打开的索引
#define IX_CLOSEDINDEX          (START_IX_WARN + 5)     // 关闭一个已经关闭的索引
#define IX_INVALIDROOTPOINTER   (START_IX_WARN + 6)     // 指向根节点的数据指针错误
#define IX_SEARCHFAILED         (START_IX_WARN + 7)     // 搜索节点失败
#define IX_INVALIDKEYNUM        (START_IX_WARN + 8)     // 每个Node内key容量不合理
#define IX_INVALIDRIDNUM        (START_IX_WARN + 9)     // 每个Bucket内rid容量不合理
#define IX_SEARCHEMPTYNODE      (START_IX_WARN + 10)    // 待查找node为空
#define IX_INSERTNODEFILED      (START_IX_WARN + 11)    // 节点无法插入
#define IX_INSERTBUCKETFILED    (START_IX_WARN + 12)    // Bucket插入失败
#define IX_DONTNEEDSPLIT        (START_IX_WARN + 13)    // 当前node不满足分裂条件
#define IX_DELETENODEFILED      (START_IX_WARN + 14)    // 节点无法删除
#define IX_FINDRIDFILED         (START_IX_WARN + 15)    // rid查找失败
#define IX_OPENEDSCAN           (START_IX_WARN + 16)    // scan已经打开
#define IX_CLOSEDSCAN           (START_IX_WARN + 17)    // scan已关闭
#define IX_INVALIDNODEHEIGHT    (START_IX_WARN + 18)    // 新建Node失败，因为level不对
#define IX_EOF                  (START_IX_WARN + 19)    // scan到达末尾

#define IX_LASTWARN             IX_EOF


#define IX_UNIX                 (START_IX_ERR - 0)
#define IX_LASTERROR            IX_UNIX

#endif // IX_H
