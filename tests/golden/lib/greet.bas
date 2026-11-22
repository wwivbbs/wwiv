' Imported by import.bas. Its top-level code runs once, when imported.
import "cycle.bas"

greeting = "Hello"
print "greet.bas loaded"

def Greet(name)
  return greeting + ", " + name
enddef
