' Syntax that WWIV scripts written for my_basic use.
' - comments with '
' - statements ending in ;
' - PRINT without parentheses, with , and ;
' - NOT, and RETURN with no value

def Check(flag)
  If Not flag Then
    Return
  EndIf
  print "flag was set"
enddef

count = 0;
Check(False)
Check(True);
print "count: ", count'trailing comment
print "a"; "b"
