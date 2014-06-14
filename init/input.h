/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#ifndef __INCLUDED_INPUT_H__
#define __INCLUDED_INPUT_H__

#include <functional>
#include <string>
#include <vector>

class BaseEditItem {
public:
  BaseEditItem(int x, int y, int maxsize)
    : x_(x), y_(y), maxsize_(maxsize) {};
  virtual ~BaseEditItem() {}

  virtual int Run() = 0;

protected:
  int x_;
  int y_;
  int maxsize_;
};


template<typename T>
class EditItem : public BaseEditItem {
public:
  EditItem(int x, int y, int maxsize, T data) : BaseEditItem(x, y, maxsize), data_(data) {};
  virtual ~EditItem() {}

protected:
  const T data() const { return data_; }
  void set_data(T data) { data_ = data; }
  T data_;
};


template<typename T> class StringEditItem : public EditItem<T> {
public:
  StringEditItem(int x, int y, int maxsize, T data) : EditItem(x, y, maxsize, data) {}
  virtual ~StringEditItem() {}

  virtual int Run();
};


template<typename T> class NumberEditItem : public EditItem<T*> {
public:
  NumberEditItem(int x, int y, T* data) : EditItem(x, y, 0, data) {}
  virtual ~NumberEditItem() {}

  virtual int Run();
};


class CustomEditItem : public BaseEditItem {
public:
  typedef std::function<std::string(void)> prefn;
  typedef std::function<void(const std::string&)> postfn;
  CustomEditItem(int x, int y, int maxsize,
    prefn to_field, postfn from_field) 
      : BaseEditItem(x, y, maxsize), 
        to_field_(to_field), from_field_(from_field) {}

  virtual int Run();

private:
  prefn to_field_;
  postfn from_field_;
};


class EditItems {
public:
  EditItems(std::initializer_list<BaseEditItem*> l) : items_(l) {}
  virtual ~EditItems();

  virtual void Run();

private:
  std::vector<BaseEditItem*> items_;
};


// Function prototypes
void PrintfY(int x, int y, const char *pszFormat, ...);

#endif // __INCLUDED_INPUT_H__
