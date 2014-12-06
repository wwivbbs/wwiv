#!/usr/bin/python

import string
import sys


def convert_template(fn):
    with open('template.h', 'r') as tmpl:
        s=tmpl.read()
        with open(fn+'.h', 'w') as out:
            part = fn.upper()
            new_s = s.replace('TEMPLATE', part)
            out.write(new_s)
            print ('created file: %s.h' % fn)

for arg in sys.argv[1:]:
    convert_template(arg)

  
