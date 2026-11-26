'
' A message viewer: lists the message areas you can read, the messages in
' an area, and reads them.
'
' A sample of the wwiv.msg and wwiv.screen modules. It needs the new BASIC
' engine (bbs --basic_engine=next, or the engine setting in wwivconfig).
'
' With ANSI it's full-screen: lightbar lists of the areas and messages, and a
' reader that scrolls. Without ANSI it's line by line.
'
' Where the caller may post, it also writes posts and replies in the
' caller's message editor. That needs "Enable MSG Writing" turned on in
' wwivconfig's scripting settings; without it the viewer only reads.
'
' Text other users wrote (titles, names, messages) is printed with
' outstr_nomci and pl_nomci, so a |{...} or |@ code in a post shows as
' written instead of being run against the reader's session. Lightbars show
' their items that way too.
'
import "@wwiv.io"
import "@wwiv.msg"
import "@wwiv.screen"
import "@wwiv.time"

' s cut or padded with spaces to show n wide. A cut keeps no colors.
def Pad(s, n)
  If len_colors(s) > n Then
    s = LEFT(strip_colors(s), n)
  EndIf
  return pad_colors(s, n)
enddef

' When a message was posted, or "--" if that's hidden.
def DateOf(h)
  If h.date = 0 Then
    return "--"
  EndIf
  return wwiv.time.format(h.date, "%m/%d/%y %H:%M")
enddef

' The index in l of the area chosen with key, or -1.
def FindArea(l, key)
  for i = 0 to len(l) - 1
    If UCASE(get(l, i).key) = key Then
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

' A list holding message headers[i] of area a, read now, or an empty list
' if it was deleted after the list was read. Message numbers shift when
' earlier posts are deleted; FIND says where this post is now (0 if it's
' gone).
def ReadMessage(a, headers, i)
  r = list()
  h = get(headers, i)
  n = a.find(h)
  If n > 0 Then
    m = a.message(n)
    ' Unless it was deleted just now.
    If m.id = h.id Then
      push(r, m)
    EndIf
  EndIf
  return r
enddef

'
' Full-screen (ANSI)
'

def AreaLine(a)
  return "|#2" + Pad(a.key, 5) + "|#1" + Pad(a.name, 50) + " |#9" + STR(a.count) + " msgs"
enddef

def HeaderLine(h)
  return "|#2" + Pad(STR(h.number), 6) + "|#1" + Pad(h.title, 30) + " |#9" + Pad(h.from, 20) + " " + DateOf(h)
enddef

' Opens a full-screen layout: a title and a second line on top, the bars,
' and help on the command line.
def OpenScreen(title, second, help)
  s = fullscreen(2)
  gotoxy(1, 1)
  outstr("|#5" + title)
  gotoxy(1, 2)
  outstr("|#9" + second)
  s.top_bar()
  s.bottom_bar("")
  s.command_line(help)
  return s
enddef

' Lines cut to show at most w wide; a line that's too wide is split, without
' its colors.
def Wrap(lines, w)
  r = list()
  for line in lines
    If len_colors(line) <= w Then
      push(r, line)
    Else
      t = strip_colors(line)
      While len(t) > w
        push(r, LEFT(t, w))
        t = MID(t, w)
      Wend
      push(r, t)
    EndIf
  next
  return r
enddef

' Writes text on screen row y, clearing the rest of the row.
def WriteRow(y, text)
  gotoxy(1, y)
  outstr_nomci(text)
  clreol()
enddef

' Shows lines from top in the content rows of screen s.
def ShowLines(s, lines, top)
  for r = 0 to s.content_height - 1
    t = ""
    If top + r < len(lines) Then
      t = get(lines, top + r)
    EndIf
    WriteRow(s.content_top + r, "|#0" + t)
  next
enddef

' Reads messages from headers[i] full-screen; returns the index of the last
' one read, or -1 if one was deleted (the list is then out of date).
def FullRead(a, headers, i)
  s = fullscreen(5)
  w = s.width - 1
  help = "|#9Up/Down/PgUp/PgDn scroll, |#2N|#9ext, |#2P|#9revious, "
  If a.can_post Then
    help = help + "|#2R|#9eply, "
  EndIf
  help = help + "|#2Q|#9uit"
  While True
    r = ReadMessage(a, headers, i)
    If len(r) = 0 Then
      s.command_line("|#6That message has been deleted. Press a key.")
      k = s.readkey()
      s.close()
      return -1
    EndIf
    m = get(r, 0)
    ' Counted as in the list, which leaves out messages you can't read.
    WriteRow(1, "|#9Msg   : |#2" + STR(i + 1) + " of " + STR(len(headers)) + " in " + Pad(a.name, 40) + " (#" + STR(m.number) + ")")
    WriteRow(2, "|#9Title : |#1" + Pad(m.title, w - 8))
    WriteRow(3, "|#9From  : |#1" + Pad(m.from, w - 8))
    WriteRow(4, "|#9To    : |#1" + Pad(m.to, w - 8))
    WriteRow(5, "|#9Date  : |#1" + DateOf(m))
    s.top_bar()
    s.command_line(help)
    lines = Wrap(m.lines(), w)
    h = s.content_height
    top = 0
    action = ""
    While action = ""
      ShowLines(s, lines, top)
      last = top + h
      If last > len(lines) Then
        last = len(lines)
      EndIf
      s.bottom_bar(STR(top + 1) + "-" + STR(last) + " of " + STR(len(lines)))
      k = UCASE(s.readkey())
      If k = "" Or k = "Q" Then
        action = "Q"
      ElseIf k = "UP" Then
        If top > 0 Then
          top = top - 1
        EndIf
      ElseIf k = "DOWN" Then
        If top + h < len(lines) Then
          top = top + 1
        EndIf
      ElseIf k = "PGUP" Then
        top = top - h
        If top < 0 Then
          top = 0
        EndIf
      ElseIf k = "PGDN" Or k = " " Then
        If top + h < len(lines) Then
          top = top + h
        ElseIf k = " " Then
          ' Space at the end goes on to the next message.
          action = "N"
        EndIf
      ElseIf k = "HOME" Then
        top = 0
      ElseIf k = "END" Then
        top = len(lines) - h
        If top < 0 Then
          top = 0
        EndIf
      ElseIf k = "N" Or k = "RIGHT" Or k = CHR(13) Then
        action = "N"
      ElseIf k = "P" Or k = "LEFT" Then
        action = "P"
      ElseIf k = "R" And a.can_post Then
        action = "R"
      EndIf
    Wend
    If action = "Q" Then
      s.close()
      return i
    ElseIf action = "R" Then
      ' The editor has the whole screen; then show this message again.
      s.close()
      n = a.reply(m)
      s = fullscreen(5)
    ElseIf action = "P" Then
      If i > 0 Then
        i = i - 1
      EndIf
    ElseIf i < len(headers) - 1 Then
      i = i + 1
    Else
      s.close()
      return i
    EndIf
  Wend
enddef

def HeaderLines(headers)
  items = list()
  for h in headers
    push(items, HeaderLine(h))
  next
  return items
enddef

' Lists headers, the messages in area a, full-screen.
def FullArea(a, headers)
  title = a.name + "|#9 (" + STR(len(headers)) + " messages)"
  second = "Num   Title                          From                 Date"
  help = "|#9Up/Down/PgUp/PgDn, |#2Enter|#9 reads, "
  If a.can_post Then
    help = help + "|#2P|#9ost, "
  EndIf
  help = help + "|#2Q|#9uit"
  s = OpenScreen(title, second, help)
  bar = lightbar(HeaderLines(headers), s.content_top, s.content_height)
  While True
    k = UCASE(bar.run())
    If k = "" Or k = "Q" Then
      s.close()
      return
    ElseIf k = CHR(13) Or (k = "P" And a.can_post) Then
      s.close()
      i = bar.selected
      If k = "P" Then
        ' The editor has the whole screen. Select the new post, if any.
        n = a.write_post()
      Else
        i = FullRead(a, headers, i)
        n = 0
      EndIf
      ' Read the list again: it may have new posts (or replies), or lost
      ' one that was deleted.
      headers = a.headers()
      If len(headers) = 0 Then
        return
      EndIf
      bar.set_items(HeaderLines(headers))
      title = a.name + "|#9 (" + STR(len(headers)) + " messages)"
      If n > 0 Then
        i = FindMessage(headers, n)
      EndIf
      If i < 0 Then
        i = 0
      EndIf
      ' Back on the last message read, or the new post.
      bar.selected = i
      s = OpenScreen(title, second, help)
    EndIf
  Wend
enddef

def AreaLines(l)
  items = list()
  for a in l
    push(items, AreaLine(a))
  next
  return items
enddef

def FullAreas()
  l = areas()
  If len(l) = 0 Then
    pl("|#6You have no message areas to read.")
    return
  EndIf
  second = "Key  Name"
  help = "|#9Up/Down/PgUp/PgDn, |#2Enter|#9 reads, |#2Q|#9uit"
  s = OpenScreen("Message Areas", second, help)
  bar = lightbar(AreaLines(l), s.content_top, s.content_height)
  While True
    k = UCASE(bar.run())
    If k = "" Or k = "Q" Then
      s.close()
      return
    ElseIf k = CHR(13) Then
      a = get(l, bar.selected)
      headers = a.headers()
      If len(headers) = 0 Then
        s.command_line("|#6There are no messages in " + a.name + ".")
      Else
        s.close()
        FullArea(a, headers)
        ' The counts may have changed.
        l = areas()
        If len(l) = 0 Then
          return
        EndIf
        bar.set_items(AreaLines(l))
        s = OpenScreen("Message Areas", second, help)
      EndIf
    EndIf
  Wend
enddef

'
' Line by line (no ANSI)
'

' How many message headers to list at a time.
def PageSize()
  return 15
enddef

' Reads a command (a line of at most n characters) in upper case.
def Command(prompt, n)
  outstr("|#9", prompt, "|#0")
  return UCASE(gets(n))
enddef

' Reads a one key command in upper case.
def KeyCommand(prompt)
  outstr("|#9", prompt, "|#0")
  c = UCASE(getkey())
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
    pl_nomci(HeaderLine(get(headers, i)))
  next
enddef

' Shows message headers[i] of area a. Returns FALSE, showing nothing, if the
' post was deleted after the list was read.
def ShowMessage(a, headers, i)
  r = ReadMessage(a, headers, i)
  If len(r) = 0 Then
    return False
  EndIf
  m = get(r, 0)
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
    If a.can_post Then
      c = KeyCommand("[N]ext, [P]revious, [R]eply, [Q]uit: ")
    Else
      c = KeyCommand("[N]ext, [P]revious, [Q]uit: ")
    EndIf
    If c = "Q" Then
      return i
    ElseIf c = "R" And a.can_post Then
      ' Then show this message again.
      n = a.reply(get(headers, i))
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
    If a.can_post Then
      c = Command("Message #, [N]ext page, [P]revious page, [W]rite a post, [Q]uit: ", 6)
    Else
      c = Command("Message #, [N]ext page, [P]revious page, [Q]uit: ", 6)
    EndIf
    If c = "Q" Or c = "" Then
      return
    ElseIf c = "W" And a.can_post Then
      If a.write_post() > 0 Then
        headers = a.headers()
      EndIf
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

def LineAreas()
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

If okansi() Then
  FullAreas()
Else
  LineAreas()
EndIf
