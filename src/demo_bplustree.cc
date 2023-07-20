//
// File:        demo_bplustree.cc
// Description: B Plus Tree demo
// Authors:     L0-0m (rzwang@mail.ustc.edu.cn)
//

//////////////////////////////////////////////////////////////////
//  在ix.h文件头部使用IX_DEBUG宏可更改 degree(如node中最大entry个数)  //
//////////////////////////////////////////////////////////////////

#include <cstdio>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <ctime>

#include "redbase.h"
#include "pf.h"
#include "rm.h"
#include "ix.h"

#define FILENAME     "bplustree"    // demo file name
#define FEW_ENTRIES  8             //  执行测试时插入的entry数量

//
// Global component manager variables
//
PF_Manager pfm;
RM_Manager rmm(pfm);
IX_Manager ixm(pfm);

//
// Function declarations
//
RC Test(void);
void PrintError(RC rc);

//
// main
//
int main()
{
    RC rc;
    rmm.DestroyFile(FILENAME);
    ixm.DestroyIndex(FILENAME, 0);

    // 方便复现测试结果
    // srand( (unsigned)time(NULL));

    if(rc = Test())
    {
        if(rc != OK_RC)
            PrintError(rc);
    }

    return 0;
}

//
// PrintError
//
// Desc: 随机插入或删除一定数目entry并打印结果。
//
RC Test(void)
{
   RC rc;
   int index=0;
   int nEntry = FEW_ENTRIES;
   IX_IndexHandle ih;

   printf("随机插入或删除一定数目entry并打印结果... \n");

   // 构建插入key
   int keys[FEW_ENTRIES];
   for(int i = 0; i < FEW_ENTRIES; ++i)
      keys[i] = rand() % 10000;

   if((rc = ixm.CreateIndex(FILENAME, index, INT, sizeof(int))) ||
      (rc = ixm.OpenIndex(FILENAME, index, ih)))
      return (rc);
   
   // 插入样例
   RID rid(1, 2);
   for(int i = 0; i < FEW_ENTRIES; ++i)
   {
      puts("\n====================================================");
      
      printf("插入 key = %d , 之后：\n", keys[i]);
      if(rc = ih.InsertEntry(keys + i, rid)) 
         return (rc);

      if(rc = ih.PrintTree())
         return (rc);
   }

   // 删除样例 TODO 打乱删除
   for(int i = 0; i < FEW_ENTRIES; ++i)
   {
      puts("\n====================================================");
      
      printf("删除 key = %d , 之后：\n", keys[i]);
      if(rc = ih.DeleteEntry(keys + i, rid)) 
         return (rc);

      if(rc = ih.PrintTree())
         return (rc);
   }
   
   if((rc = ixm.CloseIndex(ih))              ||
      (rc = ixm.DestroyIndex(FILENAME, index)))
      return (rc);

   printf("demo done\n\n");

   return (0);
}

//
// PrintError
//
// Desc: Print an error message by calling the proper component-specific
//       print-error function
//
void PrintError(RC rc)
{
   if (abs(rc) <= END_PF_WARN)
      PF_PrintError(rc);
   else if (abs(rc) <= END_RM_WARN)
      RM_PrintError(rc);
   else if (abs(rc) <= END_IX_WARN)
      IX_PrintError(rc);
   else
      std::cerr << "Error code out of range: " << rc << "\n";
}
