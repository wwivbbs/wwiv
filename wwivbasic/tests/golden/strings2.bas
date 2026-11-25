' UCASE, LCASE, SPACE, STRING, TRIM, LTRIM, RTRIM and INSTR.
print UCASE("Hello"); "/"; LCASE("Hello")
print "["; SPACE(3); "]["; STRING(3, "="); "]["; STRING(2, 42); "]"
print "["; TRIM("  x  "); "]["; LTRIM("  x  "); "]["; RTRIM("  x  "); "]"
' Positions count from 0, as in MID; -1 is not found.
s = "one two one"
print INSTR(s, "one"); " "; INSTR(s, "one", 1); " "; INSTR(s, "three")
print MID(s, INSTR(s, "two"), 3)
