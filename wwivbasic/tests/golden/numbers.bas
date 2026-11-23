' Real numbers, for the kinds of values BBS scripts work with.
uploaded_kb = 1536
downloaded_kb = 512
print "Uploaded: "; uploaded_kb / 1024; " MB"
print "Ratio: "; uploaded_kb / downloaded_kb; " to 1"
print "Ratio: "; round(downloaded_kb / uploaded_kb * 100); "%"

seconds = 125
print "Time: "; seconds / 60; " minutes ("; fix(seconds / 60); "m "; seconds mod 60; "s)"

if uploaded_kb / downloaded_kb >= 1.5 then
  print "Good ratio"
endif
