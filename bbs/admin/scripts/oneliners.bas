PRINT "|14Welcome to WWIV 5.2 OneLiners"
PRINT "|16|11Written in WWIVBasic for Eli!"

def PrintList(l)
  i = iterator(l)
  while move_next(i)
    print get(i)
  wend
enddef

' Returns the WWIV Pipe Color Code for
' the color name displayed in onliners
def PipeColor(c)
  if c = 1 then
    return "|15"
  elseif c = 2 then
    return "|01"
  elseif c = 3 then
    return "|10"
  elseif c = 4 then
    return "|12"
  elseif c = 5 then
    return "|03"
  elseif c = 6 then
    return "|07"
  elseif c = 7 then
    return "|11"
  elseif c = 8 then
    return "|14"
  elseif c = 9 then
    return "|09"
  endif
  ' Default color.
  return "|07"
enddef

wwiv.io.nl(2)
l = list()
wwiv.data.load("GLOBAL", l)

def EnterOneLiner() 
  wwiv.io.puts("1:White 2:DkBlue 3:Green 4:Red 5:Purple 6:Gray 7:Cyan 8:Yellow 9:Blue")
  wwiv.io.nl()
  wwiv.io.puts("What Color? ")

  color = wwiv.io.getkey()
  wwiv.io.nl()
  'colorcode = 
  colorcode = ASC(LEFT(color, 1)) - ASC("0")
  pipecode = PipeColor(colorcode)
  wwiv.io.puts("|10Enter Your One Liner:")
  wwiv.io.nl()
  wwiv.io.puts("|#9: ")
  s = wwiv.io.gets(72)
  return pipecode + s
enddef

def MainLoop(l)
  done = FALSE
  wwiv.printfile("oneliners");
  while not done
	PrintList(l)
	wwiv.io.puts("|#9Would you like to add an oneliner?")

	if wwiv.io.yn() then
	  s = EnterOneLiner()
	  if len(l) > 10 then
	    remove(l, 0)
	  endif
      push(l, s)
	  wwiv.data.save("GLOBAL", l) 
	else
	  done = TRUE
	endif
  wend
enddef

MainLoop(l)