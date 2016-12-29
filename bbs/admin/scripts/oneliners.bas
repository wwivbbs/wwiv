'
' Reinterpretation of the classic OneLiners originally
' written by Σ└¡ (Eli).
'
' This version is written in WWIVbasic v5.3 as a sample
' application.
'
import "@wwiv.io"

'
' Prints the list to the screen, one row at a time.
'
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
    return "|13"
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

def EnterOneLiner() 
  puts("1:White 2:DkBlue 3:Green 4:Red 5:Purple 6:Gray 7:Cyan 8:Yellow 9:Blue")
  nl()
  puts("What Color? ")

  color = getkey()
  nl()
  colorcode = ASC(LEFT(color, 1)) - ASC("0")
  pipecode = PipeColor(colorcode)
  puts("|10Enter Your One Liner:")
  nl()
  puts("|#9: ")
  s = gets(72)
  return pipecode + s
enddef

def Main()
  PRINT "|14Welcome to WWIV 5.2 OneLiners"
  PRINT "|16|11Written in WWIVBasic for Eli!"
  nl(2)
  l = list()
  wwiv.data.load("GLOBAL", l)
  done = FALSE
  cls()
  wwiv.printfile("oneliners");
  while TRUE
	PrintList(l)
	puts("|#9Would you like to add an oneliner?")

	if not yn() then
	  return
	endif
    s = EnterOneLiner()
    if len(l) > 10 then
      remove(l, 0)
    endif
    push(l, s)
    wwiv.data.save("GLOBAL", l) 
  wend
enddef

Main()
