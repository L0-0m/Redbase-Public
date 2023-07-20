//
// File:        ix_filescan.cc
// Description: IX_PrintError function
// Authors:     Renzhong Wang (rzwang@mail.ustc.edu.cn)
//

#include <cerrno>
#include <cstdio>
#include <iostream>
#include "ix_internal.h"

using namespace std;

//
// Error table
//
static char *IX_WarnMsg[] = {
  (char*)"索引类型错误",
  (char*)"类型长度错误",
  (char*)"无效的索引号",
  (char*)"Index File Hdr 位置错误",
  (char*)"打开一个已经打开的索引",
  (char*)"关闭一个已经关闭的索引",
  (char*)"指向根节点的数据指针错误",
  (char*)"搜索节点失败",
  (char*)"每个Node内key容量不合理",
  (char*)"每个Bucket内rid容量不合理",
  (char*)"待查找node为空",
  (char*)"节点无法插入",
  (char*)"Bucket插入失败",
  (char*)"当前node不满足分裂条件",
  (char*)"节点无法删除",
  (char*)"rid查找失败",
  (char*)"scan已经打开",
  (char*)"scan已关闭",
  (char*)"新建Node失败，因为level不对",
  (char*)"scan到达末尾",
};

static char *IX_ErrorMsg[] = {
  (char*)"系统错误",
};

//
// IX_PrintError
//
// Desc: Send a message corresponding to a IX return code to cerr
//       Assumes IX_UNIX is last valid IX return code
// In:   rc - return code for which a message is desired
//
void IX_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_IX_WARN && rc <= IX_LASTWARN)
    // Print warning
    cerr << "IX warning: " << IX_WarnMsg[rc - START_IX_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_IX_ERR && -rc < -IX_LASTERROR)
    // Print error
    cerr << "IX error: " << IX_ErrorMsg[-rc + START_IX_ERR] << "\n";
  else if (rc == IX_UNIX)
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  else if (rc == 0)
    cerr << "IX_PrintError called with return code of 0\n";
  else
    cerr << "IX error: " << rc << " is out of bounds\n";
}
