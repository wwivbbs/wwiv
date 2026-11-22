parser grammar BasicParser;

options {
	tokenVocab = BasicLexer;
}

// Follows directly after the standard #includes in h + cpp files.
@parser::postinclude {
#ifndef _WIN32
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
}

// Entry point.
main
  : (
    statement
    | procedureDefinition
    | importModule
    | moduleDefinition
   )* EOF
;

// End of statement. A trailing ';' is accepted (and ignored) for
// compatibility with my_basic scripts.
eos
  : SEMICOLON? NEWLINE
  ;

procedureCall
  : procedureName LPAREN (parameterList)? RPAREN
  ;

parameterList
  : expr (COMMA expr)*
;

procedureDefinition
  : DEF procedureName LPAREN (parameterDefinitionList)? RPAREN eos statements ENDDEF eos
  ;

parameterDefinitionList
  : param = id (COMMA param = id)*
;

// IMPORT "@wwiv.io" and IMPORT @wwiv.io import a native module,
// IMPORT "file.bas" imports another source file.
importModule
  : IMPORT AT ID eos
  | IMPORT STRING eos
;

moduleDefinition
  : MODULE STRING eos
;

statements
  : statement*
;

statement
    : assignmentStatement
    | ifStatement
    | forStatement
    | whileStatement
    | doUntilStatement
    | printStatement
    | procedureCall eos
    | returnStatement
    | exitStatement
    | endStatement
    | emptyStatement
;

emptyStatement : eos;

// Precedence, highest first: unary minus, * / MOD, + -, comparisons,
// NOT, AND, OR.
expr
    : procedureCall                                     # ProcCall
    | LPAREN expr RPAREN                                # Parens
    | rvalue                                            # Ident
    | INT                                               # Int
    | STRING                                            # String
    | booleanExpr                                       # Boolean
    | MINUS expr                                        # Negate
    | expr op=(STAR | SLASH | MOD) expr                 # MulDiv
    | expr op=(PLUS | MINUS) expr                       # AddSub
    | expr op=(EQ | NE | LT | LE | GT | GE) expr        # Relation
    | NOT expr                                          # Not
    | expr AND expr                                     # And
    | expr OR expr                                      # Or
;

booleanExpr
  : TRUE
  | FALSE
;

// Statements

assignmentStatement
  : LET? lvalue EQ expr eos
  ;

ifStatement
  : IF expr THEN statements
    (ELSEIF expr THEN statements)*
    (ELSE statements)?
    ENDIF eos
;

forStatement
  : FOR ID EQ expr TO expr (STEP expr)? eos statements NEXT ID? eos
;

whileStatement
  : WHILE expr eos statements WEND eos
;

doUntilStatement
  : DO eos statements UNTIL expr eos
;

// PRINT a, b; c
// Items are printed with no separator. A newline follows unless the
// statement ends with ',' or ';'.
printStatement
  : PRINT (printItem ((COMMA | SEMICOLON) printItem)* trailing=(COMMA | SEMICOLON)?)? NEWLINE
;

printItem : expr;

returnStatement
  : RETURN expr? eos
;

// EXIT (my_basic) and BREAK leave the innermost loop.
exitStatement
  : (EXIT | BREAK) eos
;

// END stops the script.
endStatement
  : END eos
;

lvalue :   variable;

id: ID;

variable :   ID;
rvalue :   ID;
procedureName : ID;
