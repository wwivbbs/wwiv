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
#include <limits>
#include <string>
#include <vector>

#include "platform/curses_io.h"

#define NUM_ONLY            1
#define UPPER_ONLY          2
#define ALL                 4


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


// Base item of an editable value, this class does not use templates.
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


// Base item of an editable value that uses templates to hold the
// value under edit.
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

  virtual int Run() {
    out->GotoXY(this->x_, this->y_);
    int return_code = 0;
    int status = uppercase_ ? UPPER_ONLY : ALL;
    editline(reinterpret_cast<char*>(this->data_), this->maxsize_, status, &return_code, "");
    return return_code;
  }

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

template<typename T, int MAXLEN = std::numeric_limits<T>::digits10> 
class NumberEditItem : public EditItem<T*> {
public:
  NumberEditItem(int x, int y, T* data) : EditItem<T*>(x, y, 0, data) {}
  virtual ~NumberEditItem() {}

  virtual int Run() {
    out->GotoXY(this->x_, this->y_);
    char s[21];
    int return_code = 0;
    sprintf(s, "%-7u", *this->data_);
    editline(s, MAXLEN + 1, NUM_ONLY, &return_code, "");
    *this->data_ = atoi(s);
    return return_code;
  }

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

struct HelpItem {
  std::string key;
  std::string description;
};

class EditItems {
public:
  typedef std::function<void(void)> additional_helpfn;
  EditItems(std::initializer_list<BaseEditItem*> l) 
    : items_(l), navigation_help_items_(StandardNavigationHelpItems()),
      editor_help_items_(StandardEditorHelpItems()), 
      edit_mode_(false) {}
  virtual ~EditItems();

  virtual void Run();
  virtual void Display() const;
  virtual void ShowHelpItems(const std::vector<HelpItem>& help_items) const;

  void set_navigation_help_items(const std::vector<HelpItem> items) { navigation_help_items_ = items; }
  void set_editor__help_items(const std::vector<HelpItem> items) { editor_help_items_ = items; }

  static std::vector<HelpItem> StandardNavigationHelpItems() {
    return { {"Esc", "Exit"}, 
        { "Enter", "Edit" },
        { "[", "Previous" },
        { "]", "Next" },
        { "{", "Previous 10" },
        { "}", "Next 10" },
    };
  }

  static std::vector<HelpItem> StandardEditorHelpItems() {
    return { {"Esc", "Exit"}, 
        { "Enter", "Edit" },
    };
  }

  static std::vector<HelpItem> ExitOnlyHelpItems() {
    return { {"Esc", "Exit"}, 
    };
  }


private:
  std::vector<BaseEditItem*> items_;
  std::vector<HelpItem> navigation_help_items_;
  std::vector<HelpItem> editor_help_items_;
  bool edit_mode_;
};

#endif // __INCLUDED_INPUT_H__
