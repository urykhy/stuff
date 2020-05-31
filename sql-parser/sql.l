%{
#include "sql.tab.h"
%}
%option noyywrap

%%
"SELECT" {return SELECT;}
"FROM"   {return FROM;}
"AS"     {return AS;}
"WHERE"  {return WHERE;}
"AND"    {return AND;}
"OR"     {return OR;}
"NOT"    {return NOT;}
"ASC"    {return ASC;}
"DESC"   {return DESC;}
"GROUP"  {return GROUP;}
"ORDER"  {return ORDER;}
"BY"     {return BY;}
"HAVING" {return HAVING;}
"LIMIT"  {return LIMIT;}
"INSERT" {return INSERT;}
"INTO"   {return INTO;}
"VALUES" {return VALUES;}
"UPDATE" {return UPDATE;}
"SET"    {return SET;}
"DELETE" {return DELETE;}

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
"!="     {return NE;}
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
