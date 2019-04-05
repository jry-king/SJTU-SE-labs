#include <stdio.h>
#include "util.h"
#include "table.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "tree.h"
#include "printtree.h"
#include "frame.h"
#include "translate.h"

//LAB5: you can modify anything you want.

// definition

static Tr_level outermost = NULL;
extern const int F_wordSize;

struct Tr_access_ {
	Tr_level level;
	F_access access;
};

struct Tr_level_ {
	F_frame frame;
	Tr_level parent;
	Temp_label name;
	Tr_accessList formals;
};

typedef struct patchList_ *patchList;
struct patchList_ 
{
	Temp_label *head; 
	patchList tail;
};

struct Cx 
{
	patchList trues; 
	patchList falses; 
	T_stm stm;
};

struct Tr_exp_ {
	enum {Tr_ex, Tr_nx, Tr_cx} kind;
	union {T_exp ex; T_stm nx; struct Cx cx; } u;
};

static F_fragList fragList = {NULL, NULL};
static F_fragList fragListHead = &fragList;
static F_fragList fragListTail = &fragList;

// function

static Tr_access Tr_Access(Tr_level level, F_access access) 
{
	Tr_access a = checked_malloc(sizeof(*a));
	a->level = level;
	a->access = access;
	return a;
}

static Tr_accessList Tr_AccessList(Tr_access head, Tr_accessList tail)
{
	Tr_accessList l = checked_malloc(sizeof(*l));
	l->head = head;
	l->tail = tail;
	return l; 
}

Tr_expList Tr_ExpList(Tr_exp head, Tr_expList tail)
{
	Tr_expList el = checked_malloc(sizeof(*el));
	el->head = head;
	el->tail = tail;
	return el;
}

Tr_level Tr_outermost(void) 
{
	if (!outermost)
	{
		outermost = Tr_newLevel(NULL, Temp_newlabel(), NULL);
	}
	return outermost;
}

// init a new level with a new frame
Tr_level Tr_newLevel(Tr_level parent, Temp_label name, U_boolList formals) 
{
	Tr_level l = checked_malloc(sizeof(*l));
	l->parent = parent;
	l->name = name;
	l->frame = F_newFrame(name, U_BoolList(TRUE, formals));		// one extra TRUE for static link

	Tr_accessList tal = NULL;
	for(F_accessList fal = F_formals(l->frame); fal; fal = fal->tail)
	{
		Tr_access a = Tr_Access(l, fal->head);
		if(l->formals)
		{
			tal->tail = Tr_AccessList(a, NULL);
			tal = tal->tail;
		}
		else
		{
			l->formals = Tr_AccessList(a, NULL);
			tal = l->formals;
		}
	}
	return l;
}

Tr_accessList Tr_formals(Tr_level level) 
{
	return level->formals;
}

// create a variable in level
Tr_access Tr_allocLocal(Tr_level level, bool escape) 
{
	return Tr_Access(level, F_allocLocal(level->frame, escape));
}

static patchList PatchList(Temp_label *head, patchList tail)
{
	patchList list;

	list = (patchList)checked_malloc(sizeof(struct patchList_));
	list->head = head;
	list->tail = tail;
	return list;
}

void doPatch(patchList tList, Temp_label label)
{
	for(; tList; tList = tList->tail)
		*(tList->head) = label;
}

patchList joinPatch(patchList first, patchList second)
{
	if(!first) return second;
	for(; first->tail; first = first->tail);
	first->tail = second;
	return first;
}

static Tr_exp Tr_Ex(T_exp ex) 
{
	Tr_exp e = (Tr_exp)checked_malloc(sizeof(*e));
	e->kind = Tr_ex;
	e->u.ex = ex;
	return e;
}

static Tr_exp Tr_Nx(T_stm nx) 
{
	Tr_exp e = (Tr_exp)checked_malloc(sizeof(*e));
	e->kind = Tr_nx;
	e->u.nx = nx;
	return e;
}

static Tr_exp Tr_Cx(patchList trues, patchList falses, T_stm stm) 
{
	Tr_exp e = (Tr_exp)checked_malloc(sizeof(*e));
	e->kind = Tr_cx;
	e->u.cx.trues = trues;
	e->u.cx.falses = falses;
	e->u.cx.stm = stm;
	return e;
}

static T_exp unEx(Tr_exp e) {
	switch (e->kind) 
	{
	case Tr_ex:
		return e->u.ex;
	case Tr_nx:
		return T_Eseq(e->u.nx, T_Const(0));
	case Tr_cx: 
		{
			Temp_temp r = Temp_newtemp();
			Temp_label t = Temp_newlabel();
			Temp_label f = Temp_newlabel();
			doPatch(e->u.cx.trues, t);
			doPatch(e->u.cx.falses, f);
			return T_Eseq(T_Move(T_Temp(r), T_Const(1)),
					T_Eseq(e->u.cx.stm,
					T_Eseq(T_Label(f),
					T_Eseq(T_Move(T_Temp(r), T_Const(0)),
					T_Eseq(T_Label(t),
					T_Temp(r))))));
		}
	}
}

static struct Cx unCx(Tr_exp e) 
{
	switch (e->kind)	// shouldn't expect to see a Tr_nx type
	{
	case Tr_ex: 
		{
			T_stm s = T_Cjump(T_ne, e->u.ex, T_Const(0), NULL, NULL);
			patchList t = PatchList(&(s->u.CJUMP.true), NULL);
			patchList f = PatchList(&(s->u.CJUMP.false), NULL);
			Tr_exp tmp = Tr_Cx(t, f, s);
			return tmp->u.cx;
		}
	case Tr_cx:
		return e->u.cx;
	}
}

static T_stm unNx(Tr_exp e) 
{
	switch (e->kind) 
	{ 
	case Tr_ex:
		return T_Exp(e->u.ex);
	case Tr_nx:
		return e->u.nx;
	case Tr_cx: 
		{
			Temp_label label = Temp_newlabel();
			doPatch(e->u.cx.trues, label);
			doPatch(e->u.cx.falses, label);
			return T_Seq(e->u.cx.stm, T_Label(label));
		}
	}
}

static T_exp followLink(Tr_level calleeLevel, Tr_level callerLevel) 
{
	T_exp e = T_Temp(F_FP());
	if (calleeLevel == callerLevel) 
	{
		return e;
	}
	while (calleeLevel != callerLevel) 
	{
		// head->static link
		F_access a = F_formals(calleeLevel->frame)->head;
		e = F_Exp(a, e);
		calleeLevel = calleeLevel->parent;
	}
	return e;
}

static T_expList Tr_transExpList(Tr_expList l) 
{
	T_expList head = NULL;
	T_expList tail = NULL;
	for (; l; l = l->tail) 
	{
		if (head) 
		{
			tail->tail = T_ExpList(unEx(l->head), NULL);
			tail = tail->tail;
		} 
		else
		{
			head = T_ExpList(unEx(l->head), NULL);
			tail = head;
		}
	}
	return head;
}

F_fragList Tr_getResult()
{
	return fragListHead->tail;
}

static void Tr_addFrag(F_frag f)
{
	fragListTail->tail = F_FragList(f, NULL);
	fragListTail = fragListTail->tail;
}

void Tr_procEntryExit(Tr_level l, Tr_exp body, Tr_accessList formals)
{
	F_frag f = F_ProcFrag(unNx(body), l->frame);
	Tr_addFrag(f);
}

// translation

Tr_exp Tr_null()
{
	return Tr_Ex(T_Const(0));
}

// translate variable

// need to get correct level
Tr_exp Tr_simpleVar(Tr_access a, Tr_level l)
{
	return Tr_Ex(F_Exp(a->access, followLink(l, a->level)));
}

Tr_exp Tr_fieldVar(Tr_exp base, int offset)
{
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(base), T_Const(offset * F_wordSize))));
}

Tr_exp Tr_subscriptVar(Tr_exp base, Tr_exp index)
{
	return Tr_Ex(T_Mem(T_Binop(T_plus, unEx(base), T_Binop(T_mul, unEx(index), T_Const(F_wordSize)))));
}

// translate expression

Tr_exp Tr_intExp(int val)
{
	return Tr_Ex(T_Const(val));
}

Tr_exp Tr_stringExp(string s)
{
	Temp_label l = Temp_newlabel();
	F_frag f = F_StringFrag(l, s);
	Tr_addFrag(f);
	return Tr_Ex(T_Name(l));
}

Tr_exp Tr_callExp(Temp_label fun, Tr_expList el, Tr_level callerLevel, Tr_level calleeLevel)
{
	T_expList args = Tr_transExpList(el);
	args = T_ExpList(followLink(calleeLevel, callerLevel->parent), args);
	return Tr_Ex(T_Call(T_Name(fun), args));
}

Tr_exp Tr_binopExp(A_oper oper, Tr_exp left, Tr_exp right)
{
	T_binOp op;
	switch(oper)
	{
	case A_plusOp:		
		op = T_plus; 
		break;
	case A_minusOp:		
		op = T_minus; 
		break;
	case A_timesOp:		
		op = T_mul;	
		break;
	case A_divideOp:	
		op = T_div; 
		break;
	}
	return Tr_Ex(T_Binop(op, unEx(left), unEx(right)));
}

// comparation
Tr_exp Tr_relopExp(A_oper oper, Tr_exp left, Tr_exp right)
{
	T_relOp op;
	switch (oper) 
	{
	case A_eqOp:	
		op = T_eq;	
		break;
	case A_neqOp:	
		op = T_ne;	
		break;
	case A_ltOp:	
		op = T_lt;	
		break;
	case A_leOp:	
		op = T_le;	
		break;
	case A_gtOp:	
		op = T_gt;	
		break;
	case A_geOp:	
		op = T_ge;	
		break;
	}
	T_stm stm = T_Cjump(op, unEx(left), unEx(right), NULL, NULL);

	patchList trues = PatchList(&stm->u.CJUMP.true, NULL);
	patchList falses = PatchList(&stm->u.CJUMP.false, NULL);
	return Tr_Cx(trues, falses, stm);
}

// string comparation
Tr_exp Tr_strCmpExp(A_oper oper, Tr_exp left, Tr_exp right)
{
	T_exp ans = F_externalCall(String("stringEqual"), T_ExpList(unEx(left), T_ExpList(unEx(right), NULL)));
	if (A_eqOp == oper)
	{
		return Tr_Ex(ans);
	}
	else
	{
		return Tr_Ex(T_Binop(T_minus, T_Const(1), ans));
	}
}

Tr_exp Tr_recordExp(int num, Tr_expList el)
{
	Temp_temp r = Temp_newtemp();
	T_stm alloc = T_Move(T_Temp(r), F_externalCall(String("malloc"), T_ExpList(T_Const(num * F_wordSize), NULL)));
	int i = num - 1;
	T_stm seq = T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(i-- * F_wordSize))),  unEx(el->head));
				
	for (el = el->tail; el; el = el->tail, i--) 
	{
		seq = T_Seq(T_Move(T_Mem(T_Binop(T_plus, T_Temp(r), T_Const(i*F_wordSize))),  unEx(el->head)), seq); 
	}
	return Tr_Ex(T_Eseq(T_Seq(alloc, seq), T_Temp(r)));
}

Tr_exp Tr_arrayExp(Tr_exp size, Tr_exp init)
{
	return Tr_Ex(F_externalCall(String("initArray"), T_ExpList(unEx(size), T_ExpList(unEx(init), NULL))));
}

Tr_exp Tr_seqExp(Tr_expList el)
{
	T_exp seq = unEx(el->head);
	for (Tr_expList ptr = el->tail; ptr; ptr = ptr->tail)
	{
		seq = T_Eseq(unNx(ptr->head), seq);
	}
	return Tr_Ex(seq);
}

Tr_exp Tr_assignExp(Tr_exp left, Tr_exp right)
{
	return Tr_Nx(T_Move(unEx(left), unEx(right)));
}

Tr_exp Tr_ifExp(Tr_exp test, Tr_exp then, Tr_exp elsee)
{
	struct Cx cond = unCx(test);
	Temp_label t = Temp_newlabel();
	Temp_label f = Temp_newlabel();
	doPatch(cond.trues, t);
	doPatch(cond.falses, f);

	if (!elsee) 
	{	
		// if-then
		return Tr_Nx(T_Seq(cond.stm, T_Seq(T_Label(t), T_Seq(unNx(then), T_Label(f)))));
	}
	else
	{
		// if-then-else
		Temp_label join = Temp_newlabel();
		T_stm joinJump = T_Jump(T_Name(join), Temp_LabelList(join, NULL));

		// return null
		if (Tr_nx == then->kind || Tr_nx == elsee->kind) 
		{ 
			return Tr_Nx(T_Seq(cond.stm, T_Seq(T_Label(t), T_Seq(unNx(then), T_Seq(joinJump, T_Seq(T_Label(f), T_Seq(unNx(elsee), T_Label(join))))))));
		}
		// have return value
		else
		{
			Temp_temp r = Temp_newtemp();
			return Tr_Ex(T_Eseq(cond.stm, T_Eseq(T_Label(t), T_Eseq(T_Move(T_Temp(r), unEx(then)), T_Eseq(joinJump, T_Eseq(T_Label(f), T_Eseq(T_Move(T_Temp(r), unEx(elsee)), T_Eseq(T_Label(join), T_Temp(r)))))))));
		}
	}
}
Tr_exp Tr_whileExp(Tr_exp test, Tr_exp body, Tr_exp done)
{
	struct Cx cond = unCx(test);
	Temp_label lbTest = Temp_newlabel();
	Temp_label lbBody = Temp_newlabel();
	Temp_label lbDone = unEx(done)->u.NAME;
	doPatch(cond.trues, lbBody);
	doPatch(cond.falses, lbDone);
	return Tr_Nx(T_Seq(T_Label(lbTest), T_Seq(cond.stm, T_Seq(T_Label(lbBody), T_Seq(unNx(body), T_Seq(T_Jump(T_Name(lbTest), Temp_LabelList(lbTest, NULL)), T_Label(lbDone)))))));
}

Tr_exp Tr_forExp(Tr_level l, Tr_access iac, Tr_exp low, Tr_exp high, Tr_exp body, Tr_exp done)
{
	Tr_exp ex_i = Tr_simpleVar(iac, l);
	T_stm istm = unNx(Tr_assignExp(ex_i, low));
	Tr_access limac = Tr_allocLocal(l, FALSE);
	Tr_exp ex_lim = Tr_simpleVar(limac, l);
	T_stm limstm = unNx(Tr_assignExp(ex_lim, high));

	T_stm whstm = T_Cjump(T_lt, unEx(ex_i), unEx(ex_lim), NULL, NULL);
	patchList trues = PatchList(&whstm->u.CJUMP.true, NULL);
	patchList falses = PatchList(&whstm->u.CJUMP.false, NULL);
	struct Cx whcond = unCx(Tr_Cx(trues, falses, whstm));
				
	Temp_label lbTest = Temp_newlabel();
	Temp_label lbBody = Temp_newlabel();
	Temp_label lbDone = unEx(done)->u.NAME;
							
	doPatch(whcond.trues, lbBody);
	doPatch(whcond.falses, lbDone);

	T_stm dobody = T_Seq(unNx(body), T_Move(unEx(ex_i), T_Binop(T_plus, unEx(ex_lim), T_Const(1))));

	T_stm circle = T_Seq(T_Label(lbTest), T_Seq(whcond.stm, T_Seq(T_Label(lbBody), T_Seq(dobody, T_Seq(T_Jump(T_Name(lbTest), Temp_LabelList(lbTest, NULL)), T_Label(lbDone))))));

	return Tr_Nx(T_Seq(T_Seq(istm, limstm), circle));
}

Tr_exp Tr_doneExp()
{
	return Tr_Ex(T_Name(Temp_newlabel()));
}

Tr_exp Tr_breakExp(Tr_exp done)
{
	Temp_label lbDone = unEx(done)->u.NAME;
	return Tr_Nx(T_Jump(unEx(done), Temp_LabelList(lbDone, NULL)));
}

