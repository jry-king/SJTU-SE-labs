
/* 
 * frame.h
 * abstract interface for frame
 * machine-independent, implemented by modules specific to target machine like x86frame.c
 */

#ifndef FRAME_H
#define FRAME_H

#include "tree.h"

// size of one word
extern const int F_wordSize;

// holds information about formal parameters and local variables
typedef struct F_frame_ *F_frame;

// describes formals and locals that may be in the frame or registers
typedef struct F_access_ *F_access;
typedef struct F_accessList_ *F_accessList;

struct F_accessList_ {F_access head; F_accessList tail;};
// F_accessList F_AccessList(F_access head, F_accessList tail);

F_frame F_newFrame(Temp_label name, U_boolList formals);		// frame constructor
Temp_label F_name(F_frame f);									// get function name of a frame
F_accessList F_formals(F_frame f);								// get formal parameters of a frame
F_access F_allocLocal(F_frame f, bool escape);					// allocate a local variable on frame

Temp_temp F_FP();

T_exp F_Exp(F_access access, T_exp fp);

T_exp F_externalCall(string s, T_expList args);

/* declaration for fragments */
typedef struct F_frag_ *F_frag;
struct F_frag_ {enum {F_stringFrag, F_procFrag} kind;
			union {
				struct {Temp_label label; string str;} stringg;
				struct {T_stm body; F_frame frame;} proc;
			} u;
};

F_frag F_StringFrag(Temp_label label, string str);
F_frag F_ProcFrag(T_stm body, F_frame frame);

typedef struct F_fragList_ *F_fragList;
struct F_fragList_ 
{
	F_frag head; 
	F_fragList tail;
};

F_fragList F_FragList(F_frag head, F_fragList tail);

// T_stm F_procEntryExit1(F_frame frame, T_stm stm);

#endif
