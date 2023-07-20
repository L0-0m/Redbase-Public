//
// File:        rm_error.cc
// Description: RM_PrintError function
// Authors:     Renzhong Wang (rzwang@mail.ustc.edu.cn)
//

#include <cerrno>
#include <cstdio>
#include <iostream>
#include "rm_internal.h"

using namespace std;

//
// Error table
//
static char *RM_WarnMsg[] = {
  (char*)"Record size is too large",
  (char*)"Record size is too small",
  (char*)"RM_File Hdr 没有存储在 page 0 上",
  (char*)"文件已经打开",
  (char*)"文件已经关闭",
  (char*)"PageNum 错误",
  (char*)"SlotNum 错误",
  (char*)"Record 未初始化",
  (char*)"Scan 已经打开",
  (char*)"Scan 已经关闭",
  (char*)"未定义的Type",
  (char*)"属性长度错误",
  (char*)"属性值offset错误",
  (char*)"未定义的运算符"
};

static char *RM_ErrorMsg[] = {
  (char*)"Invalid PC recdor name",
  (char*)"invalid file name",
  (char*)"Scan 运行错误",
  (char*)"Inconsistent bitmap in page"
};

//
// RM_PrintError
//
// Desc: Send a message corresponding to a RM return code to cerr
//       Assumes RM_UNIX is last valid RM return code
// In:   rc - return code for which a message is desired
//
void RM_PrintError(RC rc)
{
  // Check the return code is within proper limits
  if (rc >= START_RM_WARN && rc <= RM_LASTWARN)
    // Print warning
    cerr << "RM warning: " << RM_WarnMsg[rc - START_RM_WARN] << "\n";
  // Error codes are negative, so invert everything
  else if (-rc >= -START_RM_ERR && -rc < -RM_LASTERROR)
    // Print error
    cerr << "RM error: " << RM_ErrorMsg[-rc + START_RM_ERR] << "\n";
  else if (rc == RM_UNIX)
#ifdef PC
      cerr << "OS error\n";
#else
      cerr << strerror(errno) << "\n";
#endif
  else if (rc == 0)
    cerr << "RM_PrintError called with return code of 0\n";
  else
    cerr << "RM error: " << rc << " is out of bounds\n";
}
