%{
#include <sstream>
#include <string>
#include <list>
#include <iostream>

struct Query
{
    std::string             kind;
    std::list<std::string>  table;
    std::list<std::string>  items;
    std::string             where;
    std::list<std::string>  group;
    std::string             having;
    std::list<std::string>  order;
    std::string             order_mode;
    std::string             limit;
};
Query query;

std::list<std::string> temp_buffer;

template<class... T>
char* format(T... t)
{
    std::stringstream s;
    using expander = int[];
    (void)expander{0, (void(s << std::forward<T>(t)),0)...};
    temp_buffer.push_back(s.str());
    return const_cast<char*>(temp_buffer.back().c_str());
}

void print(const std::list<std::string>& d) {
    bool first = true;
    for (const auto& x : d) {
        if (!first) { std::cout << ", "; } else { first = false; }
        std::cout << x;
    }
}

%}

%union {
	char *strval;
}

%{
#include "sql.tab.h"
void yyerror(const char* s);
int yylex (void);
%}

%token SELECT FROM AS WHERE AND OR NOT ASC DESC GROUP ORDER BY HAVING LIMIT
%token INSERT INTO VALUES UPDATE SET DELETE
%token LT GT EQ ADD SUB MULT DIV MOD
%token COMMA DOT EOL

%token <strval> INTNUM
%token <strval> FLOAT
%token <strval> BOOL
%token <strval> STRING
%token <strval> NAME

%type <strval> groupby_list
%type <strval> select_expr_list
%type <strval> select_expr
%type <strval> expr
%type <strval> table_references
%type <strval> table_factor

%start stmt_list

%%

stmt_list: stmt EOL
    | stmt EOL stmt EOL
    ;

stmt: select_stmt { ; }
    ;

select_stmt: SELECT select_expr_list FROM table_references opt_where opt_groupby opt_having opt_orderby opt_limit { query.kind="select"; } ;
    ;

select_expr_list: select_expr { query.items.push_back($1); }
    | select_expr_list COMMA select_expr { query.items.push_back($3); }
    | MULT { query.items.push_back("*"); }
    ;

select_expr: expr
    | expr AS NAME  { $$=format($1, "(rename ", $3, ")"); }
    ;

table_references: table_factor
    | table_references COMMA table_factor
    ;

table_factor: NAME { query.table.push_back($1); }
    | NAME AS NAME { query.table.push_back(format($1, "(as ", $3, ")")); }
    ;

opt_where: /* nil */
    | WHERE expr { query.where.assign($2); }
    ;

groupby_list: NAME            { query.group.push_back($1); }
    | groupby_list COMMA NAME { query.group.push_back($3); }
    ;

orderby_list: NAME            { query.order.push_back($1); }
    | orderby_list COMMA NAME { query.order.push_back($3); }
    ;

opt_asc_desc: /* nil */ { query.order_mode = "default"; }
    | ASC               { query.order_mode = "asc"; }
    | DESC              { query.order_mode = "desc"; }
    ;

opt_having:  /* nil */ | HAVING expr { query.having.assign($2); } ;
opt_groupby: /* nil */ | GROUP BY groupby_list ;
opt_orderby: /* nil */ | ORDER BY orderby_list opt_asc_desc ;
opt_limit:   /* nil */ | LIMIT INTNUM { query.limit.assign($2); } ;

expr: NAME           { $$=$1; }
    | NAME DOT NAME  { $$=format($1, ".", $3); }
    | STRING         { $$=$1; }
    | INTNUM         { $$=$1; }
    | FLOAT          { $$=$1; }
    | BOOL           { $$=$1; }
    ;

expr: expr ADD expr   { $$ = format($1, " ADD ", $3); }
    | expr SUB expr   { $$ = format($1, " SUB ", $3); }
    | expr MULT expr  { $$ = format($1, " MUL ", $3); }
    | expr DIV expr   { $$ = format($1, " DIV ", $3); }
    | expr MOD expr   { $$ = format($1, " MOD ", $3); }
    | expr LT expr    { $$ = format($1, " LT ", $3); }
    | expr GT expr    { $$ = format($1, " GT ", $3); }
    | expr EQ expr    { $$ = format($1, " EQ ", $3); }
    | expr AND expr   { $$ = format($1, " AND ", $3); }
    | expr OR expr    { $$ = format($1, " OR ", $3);}
    ;
%%

void yyerror(const char* s) {
  fprintf(stderr, "ERROR: %s\n", s);
}

int
main(int ac, char **av)
{
    printf("starting... ");

    if (!yyparse()) { printf("SQL parse worked\n"); }
    else { printf("SQL parse failed\n"); return -1; }

    std::cout << "got query" << std::endl;
    std::cout << "kind: " << query.kind << std::endl;
    std::cout << "tables: "; print(query.table); std::cout << std::endl;
    std::cout << "items: ";  print(query.items); std::cout << std::endl;
    std::cout << "where: " << query.where << std::endl;
    std::cout << "group by: ";  print(query.group); std::cout << std::endl;
    std::cout << "having: " << query.having << std::endl;
    std::cout << "order by: ";  print(query.order); std::cout << std::endl;
    std::cout << "order mode: " << query.order_mode << std::endl;
    std::cout << "limit: " << query.limit << std::endl;

    return 0;
} /* main */
