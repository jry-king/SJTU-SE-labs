#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "types.h"
#include "helper.h"
#include "env.h"
#include "semant.h"

/*Lab4: Your implementation of lab4*/


typedef void* Tr_exp;
struct expty 
{
	Tr_exp exp; 
	Ty_ty ty;
};

//In Lab4, the first argument exp should always be **NULL**.
struct expty expTy(Tr_exp exp, Ty_ty ty)
{
	struct expty e;

	e.exp = exp;
	e.ty = ty;

	return e;
}

// helper function used to skip past all names and find the actual type of a name type
Ty_ty actual_ty(Ty_ty type)
{
	if(!type)
	{
		return NULL;
	}
	if(Ty_name == type->kind)
	{
		return actual_ty(type->u.name.ty);
	}
	else
	{
		return type;
	}
}

// helper function used to check whether two Ty_ty is the same
bool tycmp(Ty_ty t1, Ty_ty t2)
{
	if(!(t1&&t2))
	{
		return FALSE;
	}
	Ty_ty a1 = actual_ty(t1);
	Ty_ty a2 = actual_ty(t2);
	if(a1 != a2)
	{
		// check nil type
		if(!(Ty_record == a1->kind && Ty_nil == a2->kind) && !(Ty_nil == a1->kind && Ty_record == a2->kind))
		{
			return FALSE;
		}
	}
	return TRUE;
}

// helper function used to convert A_fieldList to Ty_tyList in transdec(fundec)
Ty_tyList afieldToTylist(S_table tenv, A_fieldList afield)
{
	if(!afield)
	{
		return NULL;
	}
	Ty_tyList tys = Ty_TyList(NULL, NULL);
	Ty_tyList res = tys;
	A_fieldList fields;
	for(fields = afield; fields; fields = fields->tail)
	{
		Ty_ty ty = S_look(tenv, fields->head->typ);
		if(!ty)
		{
			EM_error(afield->head->pos, "undefined type %s", S_name(afield->head->typ));
		}
		tys->tail = Ty_TyList(ty, NULL);
		tys = tys->tail;
	}
	return res->tail;
}

// helper function used to convert A_fieldList to Ty_fieldList in transty(recordty)
Ty_fieldList afieldToTyfield(S_table tenv, A_fieldList afield)
{
	if(NULL == afield)
	{
		return NULL;
	}
	Ty_fieldList tyfields = Ty_FieldList(NULL, NULL);
	Ty_fieldList res = tyfields;
	Ty_ty ty;
	for(; afield; afield = afield->tail)
	{
		ty = S_look(tenv, afield->head->typ);
		if(NULL == ty)
		{
			EM_error(afield->head->pos, "undefined type %s", S_name(afield->head->typ));
		}
		tyfields->tail = Ty_FieldList(Ty_Field(afield->head->name, ty), NULL);
		tyfields = tyfields->tail;
	}
	return res->tail;
}

struct expty transVar(S_table venv, S_table tenv, A_var v)
{
	fprintf(stderr, "transVar\n");
	switch(v->kind)
	{
	// a simple variable
	case A_simpleVar:
	{
		fprintf(stderr, "\tsimpleVar\n");
		E_enventry x = S_look(venv, v->u.simple);
		if(x && E_varEntry == x->kind)
		{
			return expTy(NULL, actual_ty(x->u.var.ty));
		}
		else
		{
			EM_error(v->pos, "undefined variable %s", S_name(v->u.simple));
			return expTy(NULL, Ty_Int());
		}
	}
	// one element in an array, need to evaluate both the array id and the index
	case A_subscriptVar:
	{
		fprintf(stderr, "\tsubscriptVar\n");
		struct expty varExpty = transVar(venv, tenv, v->u.subscript.var);
		struct expty expExpty = transExp(venv, tenv, v->u.subscript.exp);
		if(Ty_array != varExpty.ty->kind)
		{
			EM_error(v->pos, "array type required");
			return expTy(NULL, Ty_Int());
		}
		if(Ty_int != expExpty.ty->kind)
		{
			EM_error(v->pos, "Subscript was not an integer");
			return expTy(NULL, Ty_Int());	
		}
		return expTy(NULL, actual_ty(varExpty.ty->u.array));
	}
	// one field in a record, evaluate the record id then traverse the fieldlist of the record to find it
	case A_fieldVar:
	{
		fprintf(stderr, "\tfieldVar\n");
		struct expty varExpty = transVar(venv, tenv, v->u.field.var);
		if(Ty_record != varExpty.ty->kind)
		{
			EM_error(v->pos, "not a record type");
			return expTy(NULL, Ty_Int());
		}
		else
		{
			Ty_fieldList fields;
			for(fields = varExpty.ty->u.record; fields; fields = fields->tail)
			{
				if(v->u.field.sym == fields->head->name)
				{
					return expTy(NULL, actual_ty(fields->head->ty));
				}
			}
			EM_error(v->pos, "field %s doesn't exist", S_name(v->u.field.sym));
			return expTy(NULL, Ty_Int());
		}
	}
	}
}

struct expty transExp(S_table venv, S_table tenv, A_exp a)
{
	fprintf(stderr, "transExp\n");
	switch(a->kind)
	{
	// a variable
	case A_varExp:
		fprintf(stderr, "\tvarExp\n");
		return transVar(venv, tenv, a->u.var);
	// nil
	case A_nilExp:
		fprintf(stderr, "\tnilExp\n");
		return expTy(NULL, Ty_Nil());
	// an integer
	case A_intExp:
		fprintf(stderr, "\tintExp\n");
		return expTy(NULL, Ty_Int());
	// a string
	case A_stringExp:
		fprintf(stderr, "\tstringExp\n");
		return expTy(NULL, Ty_String());
	// a function call, need to look up the function id first, then compare the types of its formal parameters with arguments of the given expression, and return the result type
	case A_callExp:
	{
		fprintf(stderr, "\tcallExp\n");
		// look up function id
		E_enventry x = S_look(venv, a->u.call.func);
		if(NULL == x)
		{
			EM_error(a->pos, "undefined function %s", S_name(a->u.call.func));
			return expTy(NULL, Ty_Int());
		}
		if(E_funEntry != x->kind)
		{
			EM_error(a->pos, "variable %s is not a function", S_name(a->u.call.func));
			return expTy(NULL, Ty_Int());
		}
		// compare formal parameters and arguments
		Ty_tyList formals;
		A_expList args;
		for(formals = x->u.fun.formals, args = a->u.call.args; formals && args; formals = formals->tail, args = args->tail)
		{
			struct expty arg = transExp(venv, tenv, args->head);
			if(!tycmp(formals->head, arg.ty))
			{
				EM_error(a->pos, "para type mismatch");
			}
		}
		if(formals)
		{
			EM_error(a->pos, "too few params in function %s", S_name(a->u.call.func));
		}
		if(args)
		{
			EM_error(a->pos, "too many params in function %s", S_name(a->u.call.func));
		}
		// return the result type
		return expTy(NULL, x->u.fun.result);
	}
	// an arithmetic operation, need to check both sides, and the relation between them
	case A_opExp:
	{
		fprintf(stderr, "\topExp\n");
		A_oper oper = a->u.op.oper;
		struct expty left = transExp(venv, tenv, a->u.op.left);
		struct expty right = transExp(venv, tenv, a->u.op.right);
		switch(oper)
		{
		case A_plusOp:
		case A_minusOp:
		case A_timesOp:
		case A_divideOp:
		{
			if(Ty_int != left.ty->kind)
			{
				EM_error(a->u.op.left->pos, "integer required");
			}
			if(Ty_int != right.ty->kind)
			{
				EM_error(a->u.op.right->pos, "integer required");
			}
			return expTy(NULL, Ty_Int());
		}
		case A_eqOp:
		case A_neqOp:
		{
			if(!tycmp(left.ty, right.ty))
			{
				EM_error(a->u.op.right->pos, "same type required");
			}
			else if(Ty_void == left.ty->kind)
			{
				EM_error(a->u.op.left->pos, "expression had no value");
			}
			else if(Ty_void == right.ty->kind)
			{
				EM_error(a->u.op.right->pos, "expression had no value");
			}
			return expTy(NULL, Ty_Int());
		}
		case A_ltOp:
		case A_leOp:
		case A_gtOp:
		case A_geOp:
		{
			if(!tycmp(left.ty, right.ty))
			{
				EM_error(a->u.op.right->pos, "same type required");
			}
			else if(Ty_int != left.ty->kind && Ty_string != left.ty->kind)
			{
				EM_error(a->u.op.left->pos, "string or integer required");
			}
			return expTy(NULL, Ty_Int());
		}
		}
	}
	// a record, need to look up its type symbol and the whole fieldlist 
	case A_recordExp:
	{
		fprintf(stderr, "\trecordExp\n");
		// look up record type symbol
		Ty_ty record = actual_ty(S_look(tenv, a->u.record.typ));
		if(!record)
		{
			EM_error(a->pos, "undefined type %s", S_name(a->u.record.typ));
			return expTy(NULL, Ty_Int());
		}
		if(Ty_record != record->kind)
		{
			EM_error(a->pos, "'%s' was not a record type", S_name(a->u.record.typ));
			return expTy(NULL, Ty_Int());
		}
		// look up fieldlist
		Ty_fieldList fields;
		A_efieldList actualFields;
		for(fields = record->u.record, actualFields = a->u.record.fields; fields && actualFields; fields = fields->tail, actualFields = actualFields->tail)
		{
			if(fields->head->name != actualFields->head->name)
			{
				EM_error(a->pos, "need member '%s' but '%s'", S_name(fields->head->name), S_name(actualFields->head->name));
			}
			else
			{
				struct expty actualField = transExp(venv, tenv, actualFields->head->exp);
				if(!tycmp(fields->head->ty, actualField.ty))
				{
					EM_error(a->pos, "member '%s' type mismatch", S_name(fields->head->name));
				}
			}
		}
		if(fields)
		{
			EM_error(a->pos, "too few initializers for '%s'", S_name(a->u.record.typ));
		}
		else if(actualFields)
		{
			EM_error(a->pos, "too many initializers for '%s'", S_name(a->u.record.typ));
		}
		return expTy(NULL, record);
	}
	// a sequence of expressions, evaluate one by one and return the type of the last expression
	case A_seqExp:
	{
		fprintf(stderr, "\tseqExp\n");
		if(NULL == a->u.seq)
		{
			return expTy(NULL, Ty_Void());
		}
		A_expList expseq;
		for(expseq = a->u.seq; expseq; expseq = expseq->tail)
		{
			if(!expseq->tail)
			{
				return transExp(venv, tenv, expseq->head);
			}
			transExp(venv, tenv, expseq->head);
		}
	}
	// an assignment
	case A_assignExp:
	{
		fprintf(stderr, "\tassignExp\n");
		if(A_simpleVar == a->u.assign.var->kind)
		{
			E_enventry checkloop = S_look(venv, a->u.assign.var->u.simple);
			if(checkloop && checkloop->isloop)
			{
				EM_error(a->pos, "loop variable can't be assigned");
			}
		}
		struct expty varExpty = transVar(venv, tenv, a->u.assign.var);
		struct expty expExpty = transExp(venv, tenv, a->u.assign.exp);
		if(!tycmp(varExpty.ty, expExpty.ty))
		{
			EM_error(a->pos, "unmatched assign exp");
		}
		return expTy(NULL, Ty_Void());
	}
	// an if statement, need to check all three parts
	case A_ifExp:
	{
		fprintf(stderr, "\tifExp\n");
		struct expty testExpty = transExp(venv, tenv, a->u.iff.test);
		struct expty thenExpty = transExp(venv, tenv, a->u.iff.then);
		// test part is of integer type
		if(Ty_int != testExpty.ty->kind)
		{
			EM_error(a->pos, "if-exp was not an integer");
		}
		// then and else must return same type in if-then-else
		// if expression is if-then-else or if-then-nil, not if-then, so need to check if-then even after we get else expression
		if(a->u.iff.elsee)
		{
			struct expty elseExpty = transExp(venv, tenv, a->u.iff.elsee);
			if(Ty_nil == elseExpty.ty->kind)
			{
				if(Ty_nil != thenExpty.ty->kind && Ty_record != thenExpty.ty->kind)
				{
					EM_error(a->pos, "if-then exp's body must produce no value");
				}
			}
			else if(!tycmp(thenExpty.ty, elseExpty.ty))
			{
				EM_error(a->pos, "then exp and else exp type mismatch");
			}
			return expTy(NULL, thenExpty.ty);
		}
		// then must produce no value when else is absent
		else
		{
			if(Ty_void != thenExpty.ty->kind)
			{
				EM_error(a->pos, "if-then exp's body must produce no value");
			}
			return expTy(NULL, Ty_Void());
		}
	}
	// a while statement
	case A_whileExp:
	{
		fprintf(stderr, "\twhileExp\n");
		struct expty testExpty = transExp(venv, tenv, a->u.whilee.test);
		struct expty bodyExpty = transExp(venv, tenv, a->u.whilee.body);
		// test part is of integer type
		if(Ty_int != testExpty.ty->kind)
		{
			EM_error(a->pos, "while-exp was not an integer");
		}
		// body part must produce no value
		if(Ty_void != bodyExpty.ty->kind)
		{
			EM_error(a->pos, "while body must produce no value");
		}
		return expTy(NULL, Ty_Void());
	}
	// a for statement
	case A_forExp:
	{
		fprintf(stderr, "\tforExp\n");
		// low and high boundaries is integer
		struct expty lowExpty = transExp(venv, tenv, a->u.forr.lo);
		struct expty highExpty = transExp(venv, tenv, a->u.forr.hi);
		if(Ty_int != lowExpty.ty->kind || Ty_int != highExpty.ty->kind)
		{
			EM_error(a->pos, "for exp's range type is not integer");
		}
		// loop variable's scope covers only body part
		S_beginScope(venv);
		S_beginScope(tenv);
		E_enventry loop = E_VarEntry(Ty_Int());
		loop->isloop = 1;
		S_enter(venv, a->u.forr.var, loop);
		struct expty bodyExpty = transExp(venv, tenv, a->u.forr.body);
		S_endScope(tenv);
		S_endScope(venv);
		// body must produce no value
		if(Ty_void != bodyExpty.ty->kind)
		{
			EM_error(a->pos, "body exp shouldn't return a value");
		}
		return expTy(NULL, Ty_Void());
	}
	// a break expression
	case A_breakExp:
		fprintf(stderr, "\tbreakExp\n");
		return expTy(NULL, Ty_Void());
	// a let statement
	case A_letExp:
	{
		fprintf(stderr, "\tletExp\n");
		struct expty bodyExpty;
		A_decList decs;
		// scope of definitions covers only body part
		S_beginScope(venv);
		S_beginScope(tenv);
		for(decs = a->u.let.decs; decs; decs = decs->tail)
		{
			transDec(venv, tenv, decs->head);
		}
		bodyExpty = transExp(venv, tenv, a->u.let.body);
		S_endScope(tenv);
		S_endScope(venv);
		return bodyExpty;
	}
	// an array, need to look up array type symbol, size expression and init expression
	case A_arrayExp:
	{
		fprintf(stderr, "\tarrayExp\n");
		// look up array type symbol
		Ty_ty arrayty = actual_ty(S_look(tenv, a->u.array.typ));
		if(NULL == arrayty)
		{
			EM_error(a->pos, "undefined type %s", S_name(a->u.array.typ));
			return expTy(NULL, Ty_Int());
		}
		if(Ty_array != arrayty->kind)
		{
			EM_error(a->pos, "'%s' was not a array type", S_name(a->u.array.typ));
			return expTy(NULL, Ty_Int());
		}
		// look up size and init expression
		struct expty sizeExpty = transExp(venv, tenv, a->u.array.size);
		struct expty initExpty = transExp(venv, tenv, a->u.array.init);
		if(Ty_int != sizeExpty.ty->kind)
		{
			EM_error(a->pos, "array size was not an integer value");
		}
		if(!tycmp(arrayty->u.array, initExpty.ty))
		{
			EM_error(a->pos, "array init type mismatch");
		}
		return expTy(NULL, arrayty);
	}
	}
}

void transDec(S_table venv, S_table tenv, A_dec d)
{
	fprintf(stderr, "transDec\n");
	switch(d->kind)
	{
	// function declaration, can handle mutually recursive declaration
	case A_functionDec:
	{
		fprintf(stderr, "\tfunctionDec\n");
		A_fundecList funs;
		A_fundec fun;
		// put all "headers" in environment first
		for(funs = d->u.function; funs;funs = funs->tail)
		{
			// function id
			fun = funs->head;
			if(S_look(venv, fun->name))
			{
				EM_error(fun->pos, "two functions have the same name");
				continue;
			}
			// result type
			Ty_ty result;
			if(fun->result)
			{
				result = S_look(tenv, fun->result);
				if(!result)
				{
					EM_error(fun->pos, "undefined type %s in function result type", S_name(fun->result));
					result = Ty_Int();
				}
			}
			else
			{
				result = Ty_Void();
			}
			// formal parameters
			Ty_tyList params = afieldToTylist(tenv, fun->params);
			S_enter(venv, fun->name, E_FunEntry(params, result));
		}
		// process all "bodies" in environment
		for(funs = d->u.function; funs; funs = funs->tail)
		{
			// parameters
			fun = funs->head;
			S_beginScope(venv);
			A_fieldList fields;
			for(fields = fun->params; fields; fields = fields->tail)
			{
				S_enter(venv, fields->head->name, E_VarEntry(S_look(tenv, fields->head->typ)));
			}
			struct expty bodyExpty = transExp(venv, tenv, fun->body);
			// result type
			E_enventry res = S_look(venv, fun->name);
			if(Ty_void == res->u.fun.result->kind && Ty_void != bodyExpty.ty->kind)
			{
				EM_error(fun->pos, "procedure returns value");
			}
			else if(!tycmp(res->u.fun.result, bodyExpty.ty))
			{
				EM_error(fun->pos, "wrong return type");
			}
			S_endScope(venv);
		}
		break;
	}
	// variable declaration
	case A_varDec:
	{
		fprintf(stderr, "\tvarDec\n");
		struct expty initExpty = transExp(venv, tenv, d->u.var.init);
		// init and typ should be compatible
		if(d->u.var.typ)
		{
			Ty_ty type = S_look(tenv, d->u.var.typ);
			if(!type)
			{
				EM_error(d->pos, "undefined type %s", S_name(d->u.var.typ));
				return;
			}
			else
			{
				if(!tycmp(type, initExpty.ty))
				{
					EM_error(d->pos, "type mismatch");
				}
				S_enter(venv, d->u.var.var, E_VarEntry(type));
				return;
			}
		}
		// can not initialize a variable with void type, or nil type without record type specified
		if(Ty_void == initExpty.ty->kind)
		{
			EM_error(d->pos, "can not init with void type");
		}
		if(Ty_nil == initExpty.ty->kind)
		{
			EM_error(d->pos, "init should not be nil without type specified");
		}
		S_enter(venv, d->u.var.var, E_VarEntry(initExpty.ty));
		return;
	}
	// type declaration, can handle mutually recursive declaration
	case A_typeDec:
	{
		fprintf(stderr, "\ttypeDec\n");
		A_nametyList types;
		// put all "headers" in environment first
		for(types = d->u.type; types; types = types->tail)
		{
			if(S_look(tenv, types->head->name))
			{
				EM_error(d->pos, "two types have the same name");
				continue;
			}
			S_enter(tenv, types->head->name, Ty_Name(types->head->name, NULL));
		}
		// process all "bodies" in environment
		for(types = d->u.type; types; types = types->tail)
		{
			Ty_ty type = S_look(tenv, types->head->name);
			if(!type)
			{
				continue;
			}
			type->u.name.ty = transTy(tenv, types->head->ty);
		}
		// check cycle
		for(types = d->u.type; types; types = types->tail)
		{
			Ty_ty type = S_look(tenv, types->head->name);
			if(Ty_name != type->kind)
			{
				continue;
			}
			Ty_ty currentType;
			Ty_ty usedTypes[100];
			int typeIndex = 0;
			for(currentType = type->u.name.ty, typeIndex = 0; (Ty_name == currentType->kind)&&(currentType->u.name.ty); currentType = currentType->u.name.ty, typeIndex++)
			{
				// there is a cycle if current type has appeared earlier
				for(int i = 0; i < typeIndex; i++)
				{
					if(currentType == usedTypes[i])
					{
						EM_error(d->pos, "illegal type cycle");
						return;
					}
				}
				usedTypes[typeIndex] = currentType;
			}
		}
	}
	}
}

Ty_ty transTy(S_table tenv, A_ty a)
{
	fprintf(stderr, "transTy\n");
	Ty_ty ty;
	switch(a->kind)
	{
	// a name type
	case A_nameTy:
		fprintf(stderr, "\tnameTy\n");
		ty = S_look(tenv, a->u.name);
		if(NULL == ty)
		{
			EM_error(a->pos, "undefined type %s", S_name(a->u.name));
			return Ty_Int();
		}
		else
		{
			return ty;
			//return Ty_Name(a->u.name, ty);
		}
	// a record type
	case A_recordTy:
		fprintf(stderr, "\trecordTy\n");
		return Ty_Record(afieldToTyfield(tenv, a->u.record));
	// an array type
	case A_arrayTy:
		fprintf(stderr, "\tarrayTy\n");
		ty = S_look(tenv, a->u.array);
		if(NULL == ty)
		{
			EM_error(a->pos, "undefined type %s", S_name(a->u.array));
			return Ty_Array(Ty_Int());
		}
		else
		{
			return Ty_Array(S_look(tenv, a->u.array));
		}
	}
}

void SEM_transProg(A_exp exp)
{
	S_table tenv = E_base_tenv();
	S_table venv = E_base_venv();
	transExp(venv, tenv, exp);
}
