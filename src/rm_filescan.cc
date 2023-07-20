//
// File:        rm_filescan.cc
// Description: RM_FileScan class implementation
// Authors:     L0-0m (rzwang@mail.ustc.edu.cn)
//

#include "rm_internal.h"
#include "operations.h"

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RM_FileScan::RM_FileScan  ()
{
	bScanOpen = FALSE;
}

//
// 
//
// Desc: 
// In:   
// Out:
// Ret:  
//
RM_FileScan::~RM_FileScan ()
{
	// nothing
}

//
// 
//
// Desc: 根据参数进行初始化，为扫描一个已经打开的 RM_FileHandle 做准备。
// In:   
// Out:
// Ret:  
//
RC RM_FileScan::OpenScan  (const RM_FileHandle& _fileHandle,
								 AttrType       _attrType,
								 int            _attrLength,
								 int            _attrOffset,
								 CompOp         _compOp,
								 void*          _value,
								 ClientHint     _pinHint)
{
	// 不能打开一个已经打开的scan
	if(bScanOpen == TRUE)
		return (RM_OPENEDSCAN);

	// 传入handle必须已经打开file
	if(_fileHandle.bFileOpen == FALSE)
		return (RM_CLOSEDFILE);
	pRmFh = (RM_FileHandle*)&_fileHandle;   // 初始化FileHandle

	RC rc;

	// 分别对每个参数进行检查并赋值
	if((_attrType < INT)    ||
	   (_attrType > STRING))
		return (RM_UNDEFATTRTYPE);
	attrType = _attrType;

	if((_attrType == INT)    && (_attrLength != 4)   			||
	   (_attrType == STRING) && (_attrLength > MAXSTRINGLEN))        // TODO 支持新类型时需要改动
		return (RM_INVALIDATTRLEN);
	attrLength = _attrLength;

	if((_attrOffset < 0)    ||
	   (_attrOffset >= _fileHandle.hdr.recordSize ))
		return (RM_INVALIDATTROFFSET);
	attrOffset = _attrOffset;

	if((_compOp < NO_OP)    ||
	   (_compOp > GE_OP))
	   return (RM_UNDEFCOMPOP);

	// 根据比较算符初始化比较函数
	// TODO 考虑泛型实现
	switch (_compOp)
	{
		case EQ_OP: Operate = Equal;                break;
		case LT_OP: Operate = LessThan;             break;
		case GT_OP: Operate = GreaterThan;          break;
		case LE_OP: Operate = LessThanOrEqual;      break;
		case GE_OP: Operate = GreaterThanOrEqual;   break;
		case NE_OP: Operate = NotEqual;             break;
		default:    Operate = NoComp;               break;
	}

	pValue = _value;				// TODO 应该拷贝入私有变量
	currentPage = 0;                // rec内容通过调用GetNextPage从 page 1 开始
	currentSlot = RM_SLOT_EOF;      // 遍历时会从头开始

	// 设置 scan 已打开
	bScanOpen = TRUE;

	return (OK_RC);
} 

//
// GetNextRec
//
// Desc: 遍历每个不空的page，对每个page根据bitmap判断rec位置，并进行一次比较：
//       *(type *)(r + offset) op *(type *)value
//       将符合条件的rec返回。
// In:   
// Out:  rec - 符合比较条件的rec，类中有Record内容的拷贝。
// Ret:  RM return code
//
RC RM_FileScan::GetNextRec(RM_Record &rec)
{
	RC rc;
	PF_PageHandle pfPh;
	char *pPageData;
	char *pRecData;
	char *pBitmap;

	// 进入外循环遍历page（除非读到PF_EOF），初始currentPage = 0
	while( (rc = pRmFh->pfFh.GetNextPage(currentPage, pfPh)) == OK_RC )   // 从上次page置开始
	{
		// 获得当前page内容
		if(rc == pfPh.GetData(pPageData))
		  return (rc);
	  
		// 计算bitmap地址
		pBitmap = pPageData + (pRmFh->hdr.bitmapOffset);

		// 若page不空进入内循环，遍历page中rec
		if(   ((RM_PageHdr*)pPageData)->recordNum != 0   )
		{
			// 遍历直到最后一个rec
			while(   (currentSlot = GetNextRecSlot(pBitmap)) != RM_SLOT_EOF   )
			{
				// Debug
				// 位置3-5-7  对应下标2-4-6 有rec   共20个slot（最大下标19）
				// currentSlot初始化为 RM_SLOT_EOF(-1)
				// 每次先根据currentSlot(视为已访问) 更新得到nextSlot   调用GetNextRecSlot(pBitmap)
				// 当 GetNextRecSlot(pBitmap) 返回  RM_SLOT_EOF(-1) --- 没有下一个了 

				// 读取rec
				if(rc = pRmFh->GetRec(RID(currentPage, currentSlot), rec))
					return (rc);

				// 进行条件比较
				if(Operate((void*)(pRecData + attrOffset), pValue, attrType, attrLength) == TRUE)
				{
					// 返回的rec是一份拷贝，不引用内存，可Unpinned
					pRmFh->pfFh.UnpinPage(currentPage);

					// 条件满足，返回rec
					return (OK_RC);
				}
			}
		}// rec遍历结束

		// 检查slotNum遍历过程是否正常退出
		if(currentSlot < ((RM_PageHdr*)pPageData)->recordNum)
			return (RM_RECSCANERR);

		// Unpinned
		pRmFh->pfFh.UnpinPage(currentPage);

		// 当前page扫描结束，更新pageNum
		currentPage++;
	}

	// 检查page遍历是否正常结束
	if(rc != PF_EOF)
	  return (rc);

	return (RM_EOF);
} 

//
// CloseScan
//
// Desc: 参数还原，从而可被下一次scan使用
// In:   
// Out:
// Ret:  
//
RC RM_FileScan::CloseScan()
{
	// 不能关闭一个已经关闭的scan
	if(bScanOpen == FALSE)
		return (RM_CLOSEDSCAN);

	pRmFh = NULL;

	// 关闭 scan
	bScanOpen = FALSE;

	return (OK_RC);
} 

//
//  GetNextRecSlot
//
//  Desc: 根据私有成员变量 currentSlot ，在 Bitmap 中搜索下一个rec的slotNum。
//  In:
//  Out:
//  Ret:
SlotNum RM_FileScan::GetNextRecSlot(const char *pBitmap) const
{
	char temp, mask;
	int i, j;
	
	// 未访问rec的起始slot位置
	SlotNum nextSlot = currentSlot + 1;
	


	//// 循环展开以 4B 为周期，先处理完上一个周期的查找
	//// 使 nextSlot 成为 32(4B) 的倍数...

	// 按照1B查找
	for(i = nextSlot / 8; (i < pRmFh->hdr.bitmapSize) && ((nextSlot / 32) != 0); ++i)
	{
		if(pBitmap[i] != 0x00u)
		{   // 当前1B中存在rec
			temp = pBitmap[i];
			mask = 0x80;					// 1000 0000

			for(j = nextSlot % 8; j < 8; ++j, mask >>= 1)
			{
				// 若当前slot存在rec，返回slotNum
				if ((mask & temp) != 0)
					return (i * 8 + j);
			}
		}
	}

	// 访问结束，返回RM_SLOT_EOF
	if(i == pRmFh->hdr.bitmapSize)
		return (RM_SLOT_EOF);



	//// 开始新的查找，此时nextSlot已经是 32(4B) 的整数倍数了...

	// 先按4B访问
	unsigned int *bitArray = (unsigned int *)pBitmap;
	
	// 4B时的访问边界
	int limit = pRmFh->hdr.bitmapSize / 4;

	// 按照4B查找
	for(i /= 4; i < limit; ++i)
	{
		// 当前4B中存在rec
		if(bitArray[i] != 0x00000000u)
			break;
	}

	// 按1B查找
	for(i *= 4; i < pRmFh->hdr.bitmapSize; ++i)
	{
		if(pBitmap[i] != 0x00u)
		{   // 当前1B中存在rec
			temp = pBitmap[i];
			mask = 0x80;	// 1000 0000

			for(j = 0; j < 8; ++j, mask >>= 1)
			{
				if ((mask & temp) != 0)
					return (i * 8 + j);
			}
		}
	}

	// 访问结束，返回RM_SLOT_EOF
	return (RM_SLOT_EOF);
}
