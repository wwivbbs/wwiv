# How to add a new dependency:
***

1. Fork the repo to github under wwivbbs.  We use this
   as a way to version the dependencies we use, also can
   test them from there.
2. Add a submodule, usually like this:
   ```git submodule add https://github.com/wwivbbs/module deps/module```
3. Add a reference to it into the top level CMakeFile.txt file.
4. Commit the change.