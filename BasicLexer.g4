lexer grammar BasicLexer;

// Follows directly after the standard #includes in h + cpp files.
@lexer::postinclude {
#ifndef _WIN32
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
}

options { caseInsensitive=true; }

channels { CommentsChannel, DirectiveChannel }

tokens {
	DUMMY
}

INT: Digit+;
fragment Digit: [0-9];

TRUE: 'TRUE';
FALSE: 'FALSE';

// Operators
LT: '<';
LE: '<=';
GT:  '>';
GE:  '>=';
EQ: '=';
NE: '<>';
AND: 'AND';
OR: 'OR';
NOT: 'NOT';
LET: 'LET';
MOD: 'MOD';

// General Tokens
COLON: ':';
SEMICOLON: ';';
PLUS: '+';
MINUS: '-';
STAR: '*';
SLASH: '/';
LPAREN: '(';
RPAREN: ')';
COMMA: ',';
DOT: '.';
AT: '@';

// KEYWORDS

// procedures
DEF : 'DEF';
ENDDEF : 'ENDDEF';
RETURN : 'RETURN';

// control flow
IF: 'IF';
THEN: 'THEN';
ELSE: 'ELSE';
ELSEIF : 'ELSEIF';
ENDIF: 'ENDIF';

FOR: 'FOR';
NEXT: 'NEXT';
STEP: 'STEP';
TO: 'TO';
IN: 'IN';

WHILE: 'WHILE';
WEND: 'WEND';
DO: 'DO';
UNTIL: 'UNTIL';

EXIT: 'EXIT';
BREAK: 'BREAK';
END: 'END';

PRINT: 'PRINT';

// modules
MODULE: 'MODULE';
IMPORT: 'IMPORT';

STRING options { caseInsensitive=false; } : '"' ~["\r\n]* '"';
ID: (LETTER | '_') (LETTER | '_' | '.' | Digit)*;
fragment LETTER : [A-Z];

// A comment runs to the end of the line; the newline itself stays a NEWLINE
// token so the statement before the comment is still terminated.
COMMENT : '\'' ~[\r\n]* -> channel(CommentsChannel);
WS: [ \t]+ -> channel(HIDDEN);
NEWLINE: ('\r'? '\n');
