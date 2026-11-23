' Lists: my_basic's collection functions, FOR ... IN, and l(i) indexing.
l = list("apple", "banana")
push(l, "cherry")
print "count: ", len(l)
print "first: ", l(0)

for fruit in l
  print "- ", fruit
next

' Iterators, as used by existing WWIV scripts.
i = iterator(l)
while move_next(i)
  print get(i);
  print " ";
wend
print

' Lists are shared, not copied, when assigned or passed to a function.
def AddTwo(target)
  push(target, 1, 2)
enddef
nums = list()
alias = nums
AddTwo(alias)
print "nums has ", len(nums), " items"
print nums
