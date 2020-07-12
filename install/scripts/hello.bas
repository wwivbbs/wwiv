' IMPORT "@wwiv.io"
' IMPORT "@wwiv"

def PrintList(l)
	print "PrintList: " 
  	print l
	count = 0;
	i = iterator(l)
	while move_next(i)
	  count = count + 1
	  print get(i)
	wend
enddef

PRINT "|11Hello |10World"
wwiv.module_name()
wwiv.io.module_name()
PRINT "You are running version: " + wwiv.version()

'wwiv.io.puts("What is your name? ")
'name = wwiv.io.gets(41)
'PRINT "Hello: " + name

' WWIV.RUNMENU("BBSList")

l = list("a", "b", "c", "d")
print "len: ", len(l)
wwiv.data.save("USER", l)
print "l=", l, "[end]"

'd = dict(1, "one", 2, "two")
'print "dlen: ", len(d)
'wwiv.data.save("USER", d)

nl = list()
wwiv.data.load("USER", nl)
PrintList(nl)
PrintList(l)