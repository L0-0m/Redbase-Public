//
// File:        operations.h
// Description: Operations functions implementation
// Authors:     Renzhong Wang (rzwang@mail.ustc.edu.cn)
//

#ifndef OPERATIONS_H
#define OPERATIONS_H

#include "redbase.h"
#include <cstring>

//
//  Operation Functions
//
//  Desc: 7个函数用于7种比较，在初始化时会将对应函数地址赋给类中函数指针来方便调用。
//        这7个函数是全局的。
//  In:
//  Ret:  bool
inline bool Equal(void *pValue1, void *pValue2, AttrType attrType, int attrLength)
{
  switch(attrType)
  {
	case FLOAT: return (*(float *)pValue1 == *(float*)pValue2);
	case INT: return (*(int *)pValue1 == *(int *)pValue2) ;
	default:
	  return (strncmp((char *) pValue1, (char *) pValue2, attrLength) == 0); 
  }
}

inline bool LessThan(void *pValue1, void *pValue2, AttrType attrType, int attrLength)
{
  switch(attrType)
  {
	case FLOAT: return (*(float *)pValue1 < *(float*)pValue2);
	case INT: return (*(int *)pValue1 < *(int *)pValue2) ;
	default: 
	  return (strncmp((char *) pValue1, (char *) pValue2, attrLength) < 0);
  }
}

inline bool GreaterThan(void *pValue1, void *pValue2, AttrType attrType, int attrLength)
{
  switch(attrType)
  {
	case FLOAT: return (*(float *)pValue1 > *(float*)pValue2);
	case INT: return (*(int *)pValue1 > *(int *)pValue2) ;
	default: 
	  return (strncmp((char *) pValue1, (char *) pValue2, attrLength) > 0);
  }
}

inline bool LessThanOrEqual(void *pValue1, void *pValue2, AttrType attrType, int attrLength)
{
  switch(attrType)
  {
	case FLOAT: return (*(float *)pValue1 <= *(float*)pValue2);
	case INT: return (*(int *)pValue1 <= *(int *)pValue2) ;
	default: 
	  return (strncmp((char *) pValue1, (char *) pValue2, attrLength) <= 0);
  }
}

inline bool GreaterThanOrEqual(void *pValue1, void *pValue2, AttrType attrType, int attrLength)
{
  switch(attrType)
  {
	case FLOAT: return (*(float *)pValue1 >= *(float*)pValue2);
	case INT: return (*(int *)pValue1 >= *(int *)pValue2) ;
	default: 
	  return (strncmp((char *) pValue1, (char *) pValue2, attrLength) >= 0);
  }
}

inline bool NotEqual(void *pValue1, void *pValue2, AttrType attrType, int attrLength)
{
  switch(attrType)
  {
	case FLOAT: return (*(float *)pValue1 != *(float*)pValue2);
	case INT: return (*(int *)pValue1 != *(int *)pValue2) ;
	default: 
	  return (strncmp((char *) pValue1, (char *) pValue2, attrLength) != 0);
  }
}

inline bool NoComp(void *pValue1, void *pValue2, AttrType attrType, int attrLength)
{
  return TRUE;
}

#endif  // OPERATIONS_H