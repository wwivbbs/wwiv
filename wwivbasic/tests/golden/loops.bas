' WHILE/WEND, DO/UNTIL, FOR with STEP, and EXIT.
i = 0
while i < 3
  print "while ", i
  i = i + 1
wend

do
  i = i - 1
  print "do ", i
until i = 0

for j = 10 to 0 step -5
  print "for ", j
next j

for k = 1 to 100
  if k > 2 then
    exit
  endif
  print "exit test ", k
next
