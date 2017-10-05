.PHONY: clean
.SUFFIXES:
MAKEFLAGS += -r
CFLAGS += -std=c++14 -I.

all: parser
clean:
	rm sql.tab.c sql.tab.h lex.yy.c parser sql.output || true

sql.tab.c sql.tab.h: sql.y
	bison -vd sql.y
lex.yy.c: sql.l
	flex sql.l
parser: sql.tab.c lex.yy.c
	g++ ${CFLAGS} -o $@ $^
