#include "prog1.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>

// id-int table and its constructor
typedef struct table *Table_;
struct table { string id; int value; Table_ tail; };
Table_ Table(string id, int value, struct table *tail)
{
	Table_ t = malloc(sizeof(*t));
	t->id = id;
	t->value = value;
	t->tail = tail;
	return t;
}

// struct intAndTable used while interpreting expressions to avoid side effect, and constructor
typedef struct intAndTable *IntAndTable_;
struct intAndTable { int i; Table_ t; };
IntAndTable_ createIntAndTable(int num, Table_ table)
{
	IntAndTable_ iat = checked_malloc(sizeof(*iat));
	iat->i = num;
	iat->t = table;
	return iat;
}

// find and return the value of an id in table
// with some unhandled error
int lookup(Table_ t, string key)
{
	if (NULL == t)
	{
		return 0;
	}
	Table_ temp = t;
	while (0 != strcmp(temp->id, key))
	{
		if (NULL == temp->tail)
		{
			return 0;
		}
		else
		{
			temp = temp->tail;
		}
	}
	return temp->value;
}

//********************************************part1*********************************************

int maxExpArgs(A_exp exp);
int argNum(A_expList exps);
int maxSubArgs(A_expList exps);
int maxargs(A_stm stm);

// maximum number of arguments of any print statement within given expression
int maxExpArgs(A_exp exp)
{
	switch (exp->kind)
	{
	case A_idExp: case A_numExp:
		return 0;
	case A_opExp:
		return maxExpArgs(exp->u.op.left) > maxExpArgs(exp->u.op.right) ? maxExpArgs(exp->u.op.left) : maxExpArgs(exp->u.op.right);
	case A_eseqExp:
		return maxargs(exp->u.eseq.stm) > maxExpArgs(exp->u.eseq.exp) ? maxargs(exp->u.eseq.stm) : maxExpArgs(exp->u.eseq.exp);
	default:
		return 0;
	}
}

// number of expressions in given expression list
int argNum(A_expList exps)
{
	if (A_lastExpList == exps->kind)
	{
		return 1;
	}
	else
	{
		return argNum(exps->u.pair.tail) + 1;
	}
}

// maximum number of arguments of any print statement within given expression list
int maxSubArgs(A_expList exps)
{
	if (A_lastExpList == exps->kind)
	{
		return maxExpArgs(exps->u.last);
	}
	else
	{
		return maxExpArgs(exps->u.pair.head) > maxSubArgs(exps->u.pair.tail) ? maxExpArgs(exps->u.pair.head) : maxSubArgs(exps->u.pair.tail);
	}
}

int maxargs(A_stm stm)
{
	switch (stm->kind)
	{
	case A_compoundStm:
		return maxargs(stm->u.compound.stm1) > maxargs(stm->u.compound.stm2) ? maxargs(stm->u.compound.stm1) : maxargs(stm->u.compound.stm2);
	case A_assignStm:
		return maxExpArgs(stm->u.assign.exp);
	case A_printStm:
		return argNum(stm->u.print.exps) > maxSubArgs(stm->u.print.exps) ? argNum(stm->u.print.exps) : maxSubArgs(stm->u.print.exps);
	}
	return 0;
}

//*******************************************part2********************************

Table_ interpStm(A_stm s, Table_ t);
IntAndTable_ interpExp(A_exp e, Table_ t);
IntAndTable_ interpPrint(A_expList exps, Table_ t);
void interp(A_stm stm);

// interpret a statement using given table
Table_ interpStm(A_stm s, Table_ t)
{
	switch (s->kind)
	{
	case A_compoundStm:
		return interpStm(s->u.compound.stm2, interpStm(s->u.compound.stm1, t));
	case A_assignStm:
	{
		IntAndTable_ expRes = interpExp(s->u.assign.exp, t);
		return Table(s->u.assign.id, expRes->i, expRes->t);
	}
	case A_printStm:
		return interpPrint(s->u.print.exps, t)->t;
	}
}

// interpret an expression using given table
IntAndTable_ interpExp(A_exp e, Table_ t)
{
	switch (e->kind)
	{
	case A_idExp:
		return createIntAndTable(lookup(t, e->u.id), t);
	case A_numExp:
		return createIntAndTable(e->u.num, t);
	case A_opExp:
	{
		IntAndTable_ left = interpExp(e->u.op.left, t);
		IntAndTable_ right = interpExp(e->u.op.right, left->t);
		switch (e->u.op.oper)
		{
		case A_plus:
			return createIntAndTable(left->i + right->i, right->t);
		case A_minus:
			return createIntAndTable(left->i - right->i, right->t);
		case A_times:
			return createIntAndTable(left->i * right->i, right->t);
		case A_div:
			return createIntAndTable(left->i / right->i, right->t);
		}
	}
	case A_eseqExp:
		return interpExp(e->u.eseq.exp, interpStm(e->u.eseq.stm, t));
	}
}

// interpret an expression list using given table
IntAndTable_ interpPrint(A_expList exps, Table_ t)
{
	A_expList tempList = exps;
	Table_ tempTable = t;
	while (A_pairExpList == tempList->kind)
	{
		IntAndTable_ res = interpExp(tempList->u.pair.head, tempTable);
		printf("%d ", res->i);
		tempList = tempList->u.pair.tail;
		tempTable = res->t;
	}
	IntAndTable_ lastRes = interpExp(tempList->u.last, tempTable);
	printf("%d\n", lastRes->i);
	return lastRes;
}

void interp(A_stm stm)
{
	interpStm(stm, NULL);
}