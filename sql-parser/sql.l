%{
#include "sql.tab.h"
%}
%option noyywrap

%%
"select" {return SELECT;}
"from"   {return FROM;}
"as"     {return AS;}
"where"  {return WHERE;}
"and"    {return AND;}
"or"     {return OR;}
"not"    {return NOT;}
"asc"    {return ASC;}
"desc"   {return DESC;}
"group"  {return GROUP;}
"order"  {return ORDER;}
"by"     {return BY;}
"having" {return HAVING;}
"limit"  {return LIMIT;}
"insert" {return INSERT;}
"into"   {return INTO;}
"values" {return VALUES;}
"update" {return UPDATE;}
"set"    {return SET;}
"delete" {return DELETE;}

-?[0-9]+                { yylval.strval = strdup(yytext); return INTNUM; }
-?[0-9]+"."[0-9]*       { yylval.strval = strdup(yytext); return FLOAT; }
TRUE                    { yylval.strval = strdup(yytext); return BOOL; }
FALSE                   { yylval.strval = strdup(yytext); return BOOL; }

'(\\.|''|[^'\n])*'      |
\"(\\.|\"\"|[^"\n])*\"  { yylval.strval = strdup(yytext); return STRING; }

[A-Za-z][A-Za-z0-9_]*   { yylval.strval = strdup(yytext); return NAME; }

">"      {return GT;}
"<"      {return LT;}
"="      {return EQ;}
"+"      {return ADD;}
"-"      {return SUB;}
"*"      {return MULT;}
"/"      {return DIV;}
"%"      {return MOD;}
[,]      {return COMMA;}
[.]      {return DOT;}
[;]      {return EOL;}
.        {} // ignore unknown
\n       {}
%%
