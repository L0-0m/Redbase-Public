//
// ql.h
//   Query Language Component Interface
//

// This file only gives the stub for the QL component

#ifndef QL_H
#define QL_H

#include <iostream>
#include <string>
#include <set>
#include <map>
#include <stdlib.h>
#include <string.h>
#include "redbase.h"
#include "parser.h"
#include "rm.h"
#include "ix.h"
#include "sm.h"
using namespace std;

class QL_Manager {
 public:
                                              // Constructor
      QL_Manager (SM_Manager &smm, IX_Manager &ixm, RM_Manager &rmm);
      ~QL_Manager ();                         // Destructor
   RC Select (int           nSelAttrs,        // # attrs in Select clause
              const RelAttr selAttrs[],       // attrs in Select clause
              int           nRelations,       // # relations in From clause
              const char * const relations[], // relations in From clause
              int           nConditions,      // # conditions in Where clause
              const Condition conditions[]);  // conditions in Where clause
   RC Insert (const char  *relName,           // relation to insert into
              int         nValues,            // # values to insert
              const Value values[]);          // values to insert
   RC Delete (const char *relName,            // relation to delete from
              int        nConditions,         // # conditions in Where clause
              const Condition conditions[]);  // conditions in Where clause
   RC Update (const char *relName,            // relation to update
              const RelAttr &updAttr,         // attribute to update
              const int bIsValue,             // 0/1 if RHS of = is attribute/value
              const RelAttr &rhsRelAttr,      // attr on RHS of =
              const Value &rhsValue,          // value on RHS of =
              int   nConditions,              // # conditions in Where clause
              const Condition conditions[]);  // conditions in Where clause
};

//
// Print-error function
//
void QL_PrintError(RC rc);

#endif