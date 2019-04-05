#ifndef TRANSLATE_H
#define TRANSLATE_H

#include "util.h"
#include "absyn.h"
#include "temp.h"
#include "frame.h"

/* Lab5: your code below */

typedef struct Tr_exp_ *Tr_exp;

typedef struct Tr_access_ *Tr_access;

typedef struct Tr_accessList_ *Tr_accessList;
struct Tr_accessList_ {Tr_access head; Tr_accessList tail;};

typedef struct Tr_level_ *Tr_level;

typedef struct Tr_expList_ *Tr_expList;
struct Tr_expList_ {Tr_exp head; Tr_expList tail;};

Tr_level Tr_outermost(void);

Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals);

Tr_accessList Tr_formals(Tr_level level);

Tr_access Tr_allocLocal(Tr_level level, bool escape);

Tr_exp Tr_null();
// translate variable
Tr_exp Tr_simpleVar(Tr_access acc, Tr_level lev);
Tr_exp Tr_fieldVar(Tr_exp base, int off);
Tr_exp Tr_subscriptVar(Tr_exp base, Tr_exp index);
// translate expression
Tr_exp Tr_intExp(int val);
Tr_exp Tr_stringExp(string str);
Tr_exp Tr_callExp(Temp_label fun, Tr_expList el, Tr_level callerLevel, Tr_level calleeLevel);
Tr_exp Tr_binopExp(A_oper oper, Tr_exp left, Tr_exp right);
Tr_exp Tr_relopExp(A_oper oper, Tr_exp left, Tr_exp right);
Tr_exp Tr_strCmpExp(A_oper oper, Tr_exp left, Tr_exp right);
Tr_exp Tr_recordExp(int num, Tr_expList el);
Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init);
Tr_exp Tr_seqExp(Tr_expList el);
Tr_exp Tr_assignExp(Tr_exp left, Tr_exp right);	
Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee);		
Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Tr_exp done);
Tr_exp Tr_forExp(Tr_level l, Tr_access iac, Tr_exp low, Tr_exp high, Tr_exp body, Tr_exp done);
Tr_exp Tr_doneExp();
Tr_exp Tr_breakExp(Tr_exp done);

#endif
