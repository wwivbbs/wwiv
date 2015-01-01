/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

#if !defined (__INCLUDED_WCOMM_H__)
#define __INCLUDED_WCOMM_H__

#include <string>

/**
 * Base Communication Class.
 */
class WComm {
 private:
  // used by the GetLastErrorText() method
  static std::string error_text_;
  std::string remote_name_;
  std::string remote_address_;

 protected:
  bool binary_mode_;

  static const std::string GetLastErrorText();

 public:
  WComm() : binary_mode_(false) {}
  virtual ~WComm() {}

  virtual unsigned int open() = 0;
  virtual void close(bool bIsTemporary = false) = 0;
  virtual unsigned char getW() = 0;
  virtual bool dtr(bool raise) = 0;
  virtual void purgeIn() = 0;
  virtual unsigned int put(unsigned char ch) = 0;
  virtual unsigned int read(char *buffer, unsigned int count) = 0;
  virtual unsigned int write(const char *buffer, unsigned int count, bool bNoTranslation = false) = 0;
  virtual bool carrier() = 0;
  virtual bool incoming() = 0;

  virtual unsigned int GetHandle() const = 0;
  virtual unsigned int GetDoorHandle() const { return GetHandle(); }

  void SetBinaryMode(bool b) { binary_mode_ = b; }
  bool GetBinaryMode() const { return binary_mode_; }

  void ClearRemoteInformation() {
    remote_name_ = "";
    remote_address_ = "";
  }

  void SetRemoteName(std::string name) { remote_name_ = name; }
  void SetRemoteAddress(std::string address) { remote_address_ = address; }
  const std::string GetRemoteName() const { return remote_name_; }
  const std::string GetRemoteAddress() const { return remote_address_; }

 public:
  // static factory methods
  static WComm* CreateComm(unsigned int nHandle);
};



#endif  // #if !defined (__INCLUDED_WCOMM_H__)

