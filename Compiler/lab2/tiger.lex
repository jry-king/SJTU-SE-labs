%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include "util.h"
#include "tokens.h"
#include "errormsg.h"

int charPos=1;

int yywrap(void)
{
 charPos=1;
 return 1;
}

void adjust(void)
{
 EM_tokPos=charPos;
 charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

int commentLevel = 0;
int len = 0;
char str[1000];

%}

%Start COMMENT STR

%%

<INITIAL>"," {adjust(); return COMMA;}
<INITIAL>":" {adjust(); return COLON;}
<INITIAL>";" {adjust(); return SEMICOLON;}
<INITIAL>"(" {adjust(); return LPAREN;}
<INITIAL>")" {adjust(); return RPAREN;}
<INITIAL>"[" {adjust(); return LBRACK;}
<INITIAL>"]" {adjust(); return RBRACK;}
<INITIAL>"{" {adjust(); return LBRACE;}
<INITIAL>"}" {adjust(); return RBRACE;}
<INITIAL>"." {adjust(); return DOT;}
<INITIAL>"+" {adjust(); return PLUS;}
<INITIAL>"-" {adjust(); return MINUS;}
<INITIAL>"*" {adjust(); return TIMES;}
<INITIAL>"/" {adjust(); return DIVIDE;}
<INITIAL>"=" {adjust(); return EQ;}
<INITIAL>"<>" {adjust(); return NEQ;}
<INITIAL>"<" {adjust(); return LT;}
<INITIAL>"<=" {adjust(); return LE;}
<INITIAL>">" {adjust(); return GT;}
<INITIAL>">=" {adjust(); return GE;}
<INITIAL>"&" {adjust(); return AND;}
<INITIAL>"|" {adjust(); return OR;}
<INITIAL>":=" {adjust(); return ASSIGN;}
<INITIAL>"array" {adjust(); return ARRAY;}
<INITIAL>"if" {adjust(); return IF;}
<INITIAL>"then" {adjust(); return THEN;}
<INITIAL>"else" {adjust(); return ELSE;}
<INITIAL>"while" {adjust(); return WHILE;}
<INITIAL>"for" {adjust(); return FOR;}
<INITIAL>"to" {adjust(); return TO;}
<INITIAL>"do" {adjust(); return DO;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"in" {adjust(); return IN;}
<INITIAL>"end" {adjust(); return END;}
<INITIAL>"of" {adjust(); return OF;}
<INITIAL>"break" {adjust(); return BREAK;}
<INITIAL>"nil" {adjust(); return NIL;}
<INITIAL>"function" {adjust(); return FUNCTION;}
<INITIAL>"var" {adjust(); return VAR;}
<INITIAL>"type" {adjust(); return TYPE;}
<INITIAL>[a-zA-Z][a-zA-Z0-9_]* {adjust(); yylval.sval=String(yytext); return ID;}
<INITIAL>[0-9]+ {adjust(); yylval.ival=atoi(yytext); return INT;}

<INITIAL>"/*" {adjust(); commentLevel++; BEGIN COMMENT;}
<COMMENT>"/*" {adjust(); commentLevel++;}
<COMMENT>"*/" {adjust(); commentLevel--; if(0 == commentLevel){BEGIN INITIAL;}}
<COMMENT>"\n" {adjust(); EM_newline();}
<COMMENT>. {adjust();}

<INITIAL>"\"" {adjust(); len=0; BEGIN STR;}
<STR>"\\n" {charPos+=yyleng; str[len]='\n'; len++;}
<STR>"\\t" {charPos+=yyleng; str[len]='\t'; len++;}
<STR>\\^[A-Z] {
    charPos+=yyleng;
    char ctrl = yytext[2];
    str[len] = ctrl-64;
    len++;
}
<STR>\\[0-9][0-9][0-9] {charPos+=yyleng; str[len]=atoi(yytext+1); len++;}
<STR>"\\\"" {charPos+=yyleng; str[len]='\"'; len++;}
<STR>"\\\\" {charPos+=yyleng; str[len]='\\'; len++;}
<STR>\\[ \t\n\f]+\\ {charPos+=yyleng;}
<STR>"\"" {
    charPos+=yyleng;
    if(0 == len)
    {
        strcpy(str, "(null)");
        str[6] = '\0';
    }
    else
    {
        str[len] = '\0';
    }
    yylval.sval=String(str); 
    BEGIN INITIAL; 
    return STRING;
}
<STR>. {charPos+=yyleng; strcpy(str+len, yytext); len+=yyleng;}

<INITIAL>" "|"\t" {adjust();}
<INITIAL>"\n" {adjust(); EM_newline();}
<INITIAL>. {adjust(); EM_error(charPos, yytext);}
