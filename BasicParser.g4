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

// A call: name(args), where name may be dotted (wwiv.io.outstr) and is
// resolved at runtime: a module path if one matches, otherwise a variable and
// its members (obj.method).
procedureCall
  : qualifiedName LPAREN (parameterList)? RPAREN
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
  : IMPORT AT qualifiedName eos
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
    | forInStatement
    | whileStatement
    | doUntilStatement
    | printStatement
    | callChain eos
    | returnStatement
    | exitStatement
    | endStatement
    | emptyStatement
;

emptyStatement : eos;

// Precedence, highest first: unary minus, * / MOD, + -, comparisons,
// NOT, AND, OR.
expr
    : callChain                                         # ProcCall
    | LPAREN expr RPAREN                                # Parens
    | rvalue                                            # Ident
    | INT                                               # Int
    | REAL                                              # Real
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
  : FOR qualifiedName EQ expr TO expr (STEP expr)? eos statements NEXT qualifiedName? eos
;

// FOR x IN l ... NEXT runs the body once for each item of the list l.
forInStatement
  : FOR qualifiedName IN expr eos statements NEXT qualifiedName? eos
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

lvalue : qualifiedName;

id: ID;

rvalue : qualifiedName;
procedureName : ID;

// A name with optional dotted parts: x, wwiv.io, area.name. Parts after a dot
// may be keywords (wwiv.io.print, msg.next), since they name members.
qualifiedName : ID (DOT memberName)*;

// A call followed by member accesses and method calls: f().x, a.b(1).c().
callChain : procedureCall memberSuffix*;

// .name reads a member; .name(args) calls a method.
memberSuffix : DOT memberName (LPAREN (parameterList)? RPAREN)?;

// A name after a dot: an identifier or any keyword.
memberName
  : ID
  | TRUE | FALSE | AND | OR | NOT | LET | MOD | DEF | ENDDEF | RETURN | IF | THEN | ELSE
  | ELSEIF | ENDIF | FOR | NEXT | STEP | TO | IN | WHILE | WEND | DO | UNTIL | EXIT | BREAK
  | END | PRINT | MODULE | IMPORT
  ;
