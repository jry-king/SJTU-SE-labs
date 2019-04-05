#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "symbol.h"
#include "temp.h"
#include "table.h"
#include "tree.h"
#include "frame.h"

// a word is 8 byte long in 64-bit machine
const int F_wordSize = 8;
// keep 6 parameters in register at most
static const int F_regParam = 6;

// frame
struct F_frame_ {
	F_accessList formals;
	F_accessList locals;
	Temp_label name;
	int localNum;			// number of local variables
};
// variables
struct F_access_ {
	enum {inFrame, inReg} kind;
	union {
		int offset;		// inFrame
		Temp_temp reg;	// inReg
	} u;
};
F_accessList F_AccessList(F_access head, F_accessList tail)
{
	F_accessList al = (F_accessList)checked_malloc(sizeof(*al));
	al->head = head;
	al->tail = tail;
	return al;
}
// constructors
static F_access InFrame(int offset)
{
	F_access access = (F_access)checked_malloc(sizeof(*access));
	access->kind = inFrame;
	access->u.offset = offset;
	return access;
}
static F_access InReg(Temp_temp reg)
{
	F_access access = (F_access)checked_malloc(sizeof(*access));
	access->kind = inReg;
	access->u.reg = reg;
	return access;
}

// construct a new frame with a name and an escape list
F_frame F_newFrame(Temp_label name, U_boolList formals)
{
	F_frame frame = (F_frame)checked_malloc(sizeof(*frame));
	frame->formals = NULL;
	frame->locals = NULL;
	frame->name = name;
	frame->localNum = 0;

	// count numbers of parameters in register/on stack
	int regParam = 0;
	int stackParam = 1;		// initialize to 1 for static link
	F_accessList al = NULL;
	for(U_boolList bl = formals; bl; bl = bl->tail)
	{
		F_access access = NULL;
		if(!(bl->head) && regParam < F_regParam)
		{
			access = InReg(Temp_newtemp());
			regParam++;
		}
		else
		{
			access = InFrame(stackParam * F_wordSize);
			stackParam++;
		}

		if(!(frame->formals))
		{
			frame->formals = F_AccessList(access, NULL);
			al = frame->formals;
		}
		else
		{
			al->tail = F_AccessList(access, NULL);
			al = al->tail;
		}
	}

	return frame;
}

// get the function name of a frame
Temp_label F_name(F_frame f)
{
	return f->name;
}

// get formal parameters of a frame
F_accessList F_formals(F_frame f)
{
	return f->formals;
}

// allocate a local variable on frame
F_access F_allocLocal(F_frame f, bool escape)
{
	f->localNum++;
	if(escape)
	{
		return InFrame(-1 * F_wordSize * f->localNum);
	}
	else
	{
		return InReg(Temp_newtemp());
	}
}

Temp_temp F_FP()
{
	static Temp_temp temp = NULL;
	if(!temp)
	{
		temp = Temp_newtemp();
	}
	return temp;
}

T_exp F_Exp(F_access access, T_exp fp)
{
	if(inReg == access->kind)
	{
		return T_Temp(access->u.reg);
	}
	else
	{
		return T_Mem(T_Binop(T_plus, T_Const(access->u.offset), fp));
	}
}

T_exp F_externalCall(string s, T_expList args)
{
	return T_Call(T_Name(Temp_namedlabel(s)), args);
}

F_frag F_StringFrag(Temp_label label, string str) 
{
	    F_frag frag = (F_frag)checked_malloc(sizeof(*frag));
		frag->kind = F_stringFrag;
		frag->u.stringg.label = label;
		frag->u.stringg.str = str;
		return frag;
}                                                     
                                                      
F_frag F_ProcFrag(T_stm body, F_frame frame) 
{
	    F_frag frag = (F_frag)checked_malloc(sizeof(*frag));
		frag->kind = F_procFrag;
		frag->u.proc.body = body;
		frag->u.proc.frame = frame;
		return frag;                       
}                                                     
                                                      
F_fragList F_FragList(F_frag head, F_fragList tail) 
{
	    F_fragList fl = (F_fragList)checked_malloc(sizeof(*fl));
		fl->head = head;
		fl->tail = tail;
		return fl;
}                                                     

