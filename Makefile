all: compiler

compiler: lexical.l parser.y
	flex lexical.l
	bison -d parser.y
	gcc lex.yy.c parser.tab.c -o compiler -lfl

clean:
	rm -f lex.yy.c parser.tab.c parser.tab.h compiler

