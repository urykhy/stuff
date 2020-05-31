%{
#include <sstream>
#include <string>
#include <list>
#include <iostream>

struct Query
{
    struct Order {
        std::string key;
        std::string mode;

        template<class K, class M>
        Order(K k, M m)
        : key(k)
        , mode(m)
        {}
    };

    std::string             kind;
    std::list<std::string>  table;
    std::list<std::string>  items;
    std::string             where;
    std::list<std::string>  group;
    std::string             having;
    std::list<Order>        order;
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
void print(const std::list<Query::Order>& d) {
    bool first = true;
    for (const auto& x : d) {
        if (!first) { std::cout << ", "; } else { first = false; }
        std::cout << x.key << "(" << x.mode << ")";
    }
}


%}

%union {
	char *strval;
}

%{
#include "sql.tab.h"
typedef size_t yy_size_t;
typedef struct yy_buffer_state *YY_BUFFER_STATE;

extern void yyerror(const char* s);
extern int yylex (void);
extern YY_BUFFER_STATE yy_scan_string (const char * yystr);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);
%}

%token SELECT FROM AS WHERE AND OR NOT ASC DESC GROUP ORDER BY HAVING LIMIT
%token INSERT INTO VALUES UPDATE SET DELETE
%token LT GT EQ NE ADD SUB MULT DIV MOD
%token COMMA DOT EOL

%token <strval> INTNUM
%token <strval> FLOAT
%token <strval> BOOL
%token <strval> STRING
%token <strval> NAME

%type <strval> select_expr_list
%type <strval> select_expr
%type <strval> select_stmt
%type <strval> expr
%type <strval> table_references
%type <strval> table_name
%type <strval> groupby_list
%type <strval> groupby_entry
%type <strval> orderby_list
%type <strval> orderby_entry

%left AND OR ADD SUB MULT DIV MOD LT GT EQ NE

%start stmt_list

%%

stmt_list:
    | stmt EOL
    ;

stmt:
    | select_stmt { ; }
    ;

select_stmt
    : SELECT select_expr_list FROM table_references opt_where opt_groupby opt_having opt_orderby opt_limit { query.kind="select"; } ;
    ;

select_expr_list
    : select_expr { query.items.push_back($1); }
    | select_expr_list COMMA select_expr { query.items.push_back($3); }
    | MULT { query.items.push_back("*"); }
    ;

select_expr
    : expr
    | expr AS NAME { $$=format($1, "(as ", $3, ")"); }
    ;

table_references
    : table_name
    | table_references COMMA table_name
    ;

table_name
    : NAME   { query.table.push_back($1); }
    | NAME AS NAME { query.table.push_back(format($1, "(as ", $3, ")")); }
    ;

opt_where
    : WHERE expr { query.where.assign($2); }
    ;

groupby_entry
    : NAME { query.group.push_back($1); }
    ;

groupby_list
    : groupby_entry
    | groupby_list COMMA groupby_entry
    ;

orderby_entry
    : NAME      { query.order.push_back(Query::Order($1,"default")); }
    | NAME ASC  { query.order.push_back(Query::Order($1,"asc")); }
    | NAME DESC { query.order.push_back(Query::Order($1,"desc")); }
    ;

orderby_list
    : orderby_entry
    | orderby_list COMMA orderby_entry
    ;

opt_groupby: /* nil */ | GROUP BY groupby_list ;
opt_having:  /* nil */ | HAVING expr { query.having.assign($2); } ;
opt_orderby: /* nil */ | ORDER BY orderby_list ;
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
    | expr NE expr    { $$ = format($1, " NE ", $3); }
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
    const char* buffer=
    "SELECT A.B AS AB, B, TRUE, 45, 67.89 "
    "  FROM people AS P, city AS C, region "
    " WHERE AB.C > 'str' AND H != true "
    " GROUP BY E1, E2, E3 "
    "HAVING A=42 "
    " ORDER BY F1, F2 DESC, F3 ASC "
    " LIMIT 12;";

    printf("starting... ");
    YY_BUFFER_STATE yb = yy_scan_string(buffer);
    if (!yyparse()) { printf("SQL parse worked\n"); }
    else { printf("SQL parse failed\n"); return -1; }
    yy_delete_buffer(yb);

    std::cout << "got query" << std::endl;
    std::cout << "kind: " << query.kind << std::endl;
    std::cout << "tables: "; print(query.table); std::cout << std::endl;
    std::cout << "items: ";  print(query.items); std::cout << std::endl;
    std::cout << "where: " << query.where << std::endl;
    std::cout << "group by: ";  print(query.group); std::cout << std::endl;
    std::cout << "having: " << query.having << std::endl;
    std::cout << "order by: ";  print(query.order); std::cout << std::endl;
    std::cout << "limit: " << query.limit << std::endl;

    return 0;
} /* main */
