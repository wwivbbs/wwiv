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


// Function prototypes
void Puts(const char *pszText);
void PutsXY(int x, int y, const char *pszText);
void Printf(const char *pszFormat, ...);
void PrintfXY(int x, int y, const char *pszFormat, ...);
void nlx(int numLines = 1);

bool dialog_yn(const std::string prompt);
int input_number(int max_digits);
char onek(const char *s);
void editline(std::string* s, int len, int status, int *returncode, const char *ss);
void editline(char *s, int len, int status, int *returncode, const char *ss);
int toggleitem(int value, const char **strings, int num, int *returncode);
void pausescr();

int GetNextSelectionPosition(int nMin, int nMax, int nCurrentPos, int nReturnCode);
void input_password(const std::string prompt, char *out, int max_length);
void input_password(const std::string prompt, const std::vector<std::string>& text, char *output, int max_length);
void messagebox(const std::string text);
void messagebox(const std::vector<std::string>& text);



class BaseEditItem {
public:
  BaseEditItem(int x, int y, int maxsize)
    : x_(x), y_(y), maxsize_(maxsize) {};
  virtual ~BaseEditItem() {}

  virtual int Run() = 0;
  virtual void Display() const = 0;

protected:
  int x_;
  int y_;
  int maxsize_;
};


template<typename T>
class EditItem : public BaseEditItem {
public:
  typedef std::function<std::string(void)> displayfn;

  EditItem(int x, int y, int maxsize, T data) : BaseEditItem(x, y, maxsize), data_(data) {};
  virtual ~EditItem() {}

  void set_displayfn(displayfn f) { display_ = f; }
  virtual void Display(const std::string& custom) const {
    std::string blanks(maxsize_, ' ');
    PutsXY(x_, y_, blanks.c_str());
    PutsXY(x_, y_, custom.c_str());
  };

  virtual void Display() const { 
    if (display_) { 
      Display(display_()); 
    } else {
      DefaultDisplay();
    }
  }

protected:
  virtual void DefaultDisplay() const = 0;

  const T data() const { return data_; }
  void set_data(T data) { data_ = data; }
  T data_;
  displayfn display_;
};


template<typename T> class StringEditItem : public EditItem<T> {
public:
  StringEditItem(int x, int y, int maxsize, T data, bool uppercase) 
    : EditItem<T>(x, y, maxsize, data), uppercase_(uppercase) {}
  virtual ~StringEditItem() {}

  virtual int Run();

protected:
  virtual void DefaultDisplay() const {
    std::string blanks(this->maxsize_, ' ');
    PutsXY(this->x_, this->y_, blanks.c_str());

    char pattern[81];
    sprintf(pattern, "%%-%ds", this->maxsize_);
    PrintfXY(this->x_, this->y_, pattern, this->data_);
  }
private:
  bool uppercase_;
};


template<typename T> class NumberEditItem : public EditItem<T*> {
public:
  NumberEditItem(int x, int y, T* data) : EditItem<T*>(x, y, 0, data) {}
  virtual ~NumberEditItem() {}

  virtual int Run();
protected:
  virtual void DefaultDisplay() const {
    std::string blanks(this->maxsize_, ' ');
    PutsXY(this->x_, this->y_, blanks.c_str());
    PrintfXY(this->x_, this->y_, "%-7d", *this->data_);
  }
};


class CustomEditItem : public BaseEditItem {
public:
  typedef std::function<void(const std::string&)> displayfn;
  typedef std::function<std::string(void)> prefn;
  typedef std::function<void(const std::string&)> postfn;
  CustomEditItem(int x, int y, int maxsize,
    prefn to_field, postfn from_field) 
      : BaseEditItem(x, y, maxsize), 
        to_field_(to_field), from_field_(from_field) {}

  virtual int Run();
  virtual void Display() const;
  void set_displayfn(CustomEditItem::displayfn f) { display_ = f; }

private:
  prefn to_field_;
  postfn from_field_;
  displayfn display_;
};


class EditItems {
public:
  typedef std::function<void(void)> additional_helpfn;
  EditItems(std::initializer_list<BaseEditItem*> l) : items_(l) {}
  virtual ~EditItems();

  void set_additional_helpfn(additional_helpfn f) { additional_helpfn_ = f; }
  virtual void Run();
  virtual void Display() const;
  virtual void ShowHelp() const;

private:
  std::vector<BaseEditItem*> items_;
  additional_helpfn additional_helpfn_;
};

#endif // __INCLUDED_INPUT_H__
