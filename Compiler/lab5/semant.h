#ifndef __SEMANT_H_
#define __SEMANT_H_

#include "absyn.h"
#include "symbol.h"
#include "temp.h"
#include "frame.h"
#include "translate.h"

struct expty;

struct expty transVar(Tr_level tlevel, Tr_exp texp, S_table venv, S_table tenv, A_var v);
struct expty transExp(Tr_level tlevel, Tr_exp texp, S_table venv, S_table tenv, A_exp a);
Tr_exp transDec(Tr_level tlevel, Tr_exp texp, S_table venv, S_table tenv, A_dec d);
Ty_ty  transTy (S_table tenv, A_ty a);

//void SEM_transProg(A_exp exp);
F_fragList SEM_transProg(A_exp exp);

#endif
