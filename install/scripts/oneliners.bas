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
  While move_next(i)
    Print get(i)
  Wend
enddef

' Returns the WWIV Pipe Color Code for
' the color name displayed in onliners
def PipeColor(c)
  If c = 1 then
    return "|15"
  ElseIf c = 2 then
    return "|01"
  ElseIf c = 3 then
    return "|10"
  ElseIf c = 4 then
    return "|12"
  ElseIf c = 5 then
    return "|13"
  ElseIf c = 6 then
    return "|07"
  ElseIf c = 7 then
    return "|11"
  ElseIf c = 8 then
    return "|14"
  ElseIf c = 9 then
    return "|09"
  endif
  ' Default color.
  return "|07"
enddef

def EnterOneLiner()
  outstr("|15 1:White|01 2:DkBlue|10 3:Green|12 4:Red|13 5:Purple|07 6:Gray|11 7:Cyan|14 8:Yellow|09 9:Blue")
  nl()
  outstr("What Color? ")

  color = getkey()
  nl()
  colorcode = ASC(LEFT(color, 1)) - ASC("0")
  pipecode = PipeColor(colorcode)
  outstr("|10Enter Your One Liner:")
  nl()
  outstr("|#9: ")
  s = gets(72)

  wwiv.io.outstr("|10Anonymous? ")
  an = wwiv.io.ny()
  If an Then
    name = "Anonymous"
  Else
    namepart = wwiv.interpret("N")
    number = wwiv.interpret("#")
    name = namepart + " #" + number
  EndIf
  Return pipecode + name + " - " + s
Enddef

def Main()
  l = list()
  wwiv.data.load("GLOBAL", l)
  done = False
  cls()
  While True
    wwiv.io.printfile("oneliner");
    PrintList(l)
    nl()
    outstr("|#9Would you like to add an oneliner? ")

    If Not yn() Then
      Return
    EndIf
    s = EnterOneLiner()
    If len(l) > 10 Then
      Remove(l, 0)
    EndIf
    Push(l, s)
    wwiv.data.save("GLOBAL", l) 
  wend
enddef

Main()
