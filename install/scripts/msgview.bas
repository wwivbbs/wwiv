'
' A message viewer: lists the message areas you can read, the messages in
' an area, and reads them.
'
' A sample of the wwiv.msg module. It needs the new BASIC engine (bbs
' --basic_engine=next, or the engine setting in wwivconfig).
'
' Text other users wrote (titles, names, messages) is printed with
' pl_nomci and outstr_nomci, so a |{...} or |@ code in a post shows as
' written instead of being run against the reader's session.
'
import "@wwiv.io"
import "@wwiv.msg"
import "@wwiv.time"

' How many message headers to list at a time.
def PageSize()
  return 15
enddef

' s in upper case (only a to z change).
def Upper(s)
  r = ""
  for i = 0 to len(s) - 1
    c = MID(s, i, 1)
    If ASC(c) >= ASC("a") And ASC(c) <= ASC("z") Then
      c = CHR(ASC(c) - 32)
    EndIf
    r = r + c
  next
  return r
enddef

' s cut or padded with spaces to n characters.
def Pad(s, n)
  s = s + "                                        "
  return LEFT(s, n)
enddef

' When a message was posted, or "--" if that's hidden.
def DateOf(h)
  If h.date = 0 Then
    return "--"
  EndIf
  return wwiv.time.format(h.date, "%m/%d/%y %H:%M")
enddef

' Reads a command (a line of at most n characters) in upper case.
def Command(prompt, n)
  outstr("|#9", prompt, "|#0")
  return Upper(gets(n))
enddef

' Reads a one key command in upper case.
def KeyCommand(prompt)
  outstr("|#9", prompt, "|#0")
  c = Upper(getkey())
  pl(c)
  return c
enddef

def ShowAreas(l)
  cls()
  pl("|#5Message Areas")
  nl()
  for a in l
    pl("|#2" + Pad(a.key, 5) + "|#1" + Pad(a.name, 40) + "|#9" + STR(a.count) + " msgs")
  next
enddef

' The index in l of the area chosen with key, or -1.
def FindArea(l, key)
  for i = 0 to len(l) - 1
    If Upper(get(l, i).key) = key Then
      return i
    EndIf
  next
  return -1
enddef

' The index in headers of message number n, or -1.
def FindMessage(headers, n)
  for i = 0 to len(headers) - 1
    If get(headers, i).number = n Then
      return i
    EndIf
  next
  return -1
enddef

' Lists headers first to first + PageSize() - 1.
def ShowHeaders(a, headers, first)
  cls()
  pl("|#5" + a.name + "|#9 (" + STR(len(headers)) + " messages)")
  nl()
  pl("|#9Num   Title                          From                 Date")
  last = first + PageSize() - 1
  If last > len(headers) - 1 Then
    last = len(headers) - 1
  EndIf
  for i = first to last
    h = get(headers, i)
    pl_nomci("|#2" + Pad(STR(h.number), 6) + "|#1" + Pad(h.title, 30) + " |#9" + Pad(h.from, 20) + " |#9" + DateOf(h))
  next
enddef

' Shows message headers[i] of area a.
' Shows message headers[i] of area a. Returns FALSE, showing nothing, if the
' post was deleted after the list was read.
def ShowMessage(a, headers, i)
  h = get(headers, i)
  ' Message numbers shift when earlier posts are deleted; FIND says where
  ' this post is now (0 if it's gone).
  n = a.find(h)
  If n = 0 Then
    return False
  EndIf
  m = a.message(n)
  If m.id <> h.id Then
    ' Deleted just now.
    return False
  EndIf
  cls()
  ' Counted as in the list, which leaves out messages you can't read.
  pl("|#9Msg   : |#2" + STR(i + 1) + " of " + STR(len(headers)) + " in " + a.name + " (#" + STR(m.number) + ")")
  pl_nomci("|#9Title : |#1" + m.title)
  pl_nomci("|#9From  : |#1" + m.from)
  pl_nomci("|#9To    : |#1" + m.to)
  pl("|#9Date  : |#1" + DateOf(m))
  nl()
  for line in m.lines()
    ' The text keeps its color codes, which still show with MCI off.
    pl_nomci("|#0" + line)
  next
  return True
enddef

' Reads messages from headers[i]; returns the index of the last one read, or
' -1 if one was deleted (the list is then out of date).
def ReadFrom(a, headers, i)
  While True
    If Not ShowMessage(a, headers, i) Then
      nl()
      pl("|#6That message has been deleted.")
      pause()
      return -1
    EndIf
    nl()
    c = KeyCommand("[N]ext, [P]revious, [Q]uit: ")
    If c = "Q" Then
      return i
    ElseIf c = "P" Then
      If i > 0 Then
        i = i - 1
      EndIf
    ElseIf i < len(headers) - 1 Then
      i = i + 1
    Else
      return i
    EndIf
  Wend
enddef

def ReadArea(a)
  headers = a.headers()
  If len(headers) = 0 Then
    nl()
    pl("|#6There are no messages in " + a.name + ".")
    pause()
    return
  EndIf
  first = 0
  While True
    ShowHeaders(a, headers, first)
    nl()
    c = Command("Message #, [N]ext page, [P]revious page, [Q]uit: ", 6)
    If c = "Q" Or c = "" Then
      return
    ElseIf c = "N" Then
      If first + PageSize() < len(headers) Then
        first = first + PageSize()
      EndIf
    ElseIf c = "P" Then
      first = first - PageSize()
      If first < 0 Then
        first = 0
      EndIf
    Else
      i = FindMessage(headers, VAL(c))
      If i < 0 Then
        pl("|#6There is no message " + c + " to read.")
        pause()
      Else
        i = ReadFrom(a, headers, i)
        If i < 0 Then
          ' Read the list again, and start from its first page.
          headers = a.headers()
          If len(headers) = 0 Then
            return
          EndIf
          i = 0
        EndIf
        ' Go back to the page with the last message read.
        While i >= first + PageSize()
          first = first + PageSize()
        Wend
        While i < first
          first = first - PageSize()
        Wend
      EndIf
    EndIf
  Wend
enddef

def Main()
  While True
    l = areas()
    If len(l) = 0 Then
      pl("|#6You have no message areas to read.")
      return
    EndIf
    ShowAreas(l)
    nl()
    c = Command("Area to read, or [Q]uit: ", 5)
    i = FindArea(l, c)
    If i >= 0 Then
      ReadArea(get(l, i))
    ElseIf c = "Q" Or c = "" Then
      return
    Else
      pl("|#6There is no area " + c + ".")
      pause()
    EndIf
  Wend
enddef

Main()
