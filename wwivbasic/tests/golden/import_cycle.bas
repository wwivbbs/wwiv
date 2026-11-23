' A file that imports the entry script back doesn't reload it (it used to
' crash). The second spelling of the same file is also recognized.
print "main start"
import "lib/back.bas"
import "./lib/../lib/back.bas"
print "main end"
