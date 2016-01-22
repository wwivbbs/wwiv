#!/usr/bin/env python

from distutils.core import setup, Extension
import sys

if sys.platform == "win32":
    ext = Extension("cryptlib_py",
                    sources=["python.c"],
                    library_dirs=['../binaries'],
                    libraries=['cl32'])
else:
    ext = Extension("cryptlib_py",
                    sources=["python.c"],
                    library_dirs=['..'],
                    libraries=['cl'])

setup(name="cryptlib_py", ext_modules=[ext])
