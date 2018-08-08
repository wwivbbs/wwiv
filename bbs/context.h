/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                  Copyright (C)2018, WWIV Software Services             */
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
/**************************************************************************/
#ifndef __INCLUDED_BBS_CONTEXT_H__
#define __INCLUDED_BBS_CONTEXT_H__

#include <cstdint>
#include <memory>
#include <string>

#include "core/wwivport.h"

class Application;

namespace wwiv {
namespace bbs {

class SessionContext {
public:
  SessionContext(Application* a);
  virtual ~SessionContext() = default;

  /**
   * Initializes an empty context, called after Config.dat
   * has been read and processed.
   */
  void InitalizeContext();
  /**
   * Clears the qscan pointers.
   */
  void ResetQScanPointers();
  /**
   * Resets the application level context.
   */
  void reset();
  bool ok_modem_stuff() const noexcept { return ok_modem_stuff_; }
  void ok_modem_stuff(bool o) { ok_modem_stuff_ = o; }

  bool incom() const noexcept { return incom_; }
  void incom(bool i) { incom_ = i; }
  bool outcom() const noexcept { return outcom_; }
  void outcom(bool o) { outcom_ = o; }

  bool okmacro() const noexcept { return okmacro_; }
  void okmacro(bool o) { okmacro_ = o; }

  bool forcescansub() const noexcept { return forcescansub_; }
  void forcescansub(bool g) { forcescansub_ = g; }
  bool guest_user() const noexcept { return guest_user_; }
  void guest_user(bool g) { guest_user_ = g; }

  daten_t nscandate() const noexcept { return nscandate_; }
  void nscandate(daten_t d) { nscandate_ = d; }

  // qsc is the qscan pointer. The 1st 4 bytes are the sysop sub number.
  uint32_t* qsc{nullptr};
  // A bitfield controlling if the directory should be included in the new scan.
  uint32_t* qsc_n{nullptr};
  // A bitfield contrlling if the sub should be included in the new scan.
  uint32_t* qsc_q{nullptr};
  // Array of 32-bit unsigned integers for the qscan pointer value
  // aka high message read pointer) for each sub.
  uint32_t* qsc_p{nullptr};

  const std::string irt() const { return std::string(irt_); }
  void irt(const std::string& irt);
  void clear_irt() { irt_[0] = '\0'; }

  // TODO(rushfan): Move this to private later
  char irt_[81];

private:
  Application* a_;
  std::unique_ptr<uint32_t[]> qscan_;

  bool ok_modem_stuff_{false};
  bool incom_{false};
  bool outcom_{false};
  bool okmacro_{true};
  bool forcescansub_{false};
  bool guest_user_{false};
  daten_t nscandate_{0};
};

} // namespace bbs
} // namespace wwiv

#endif // __INCLUDED_BBS_CONTEXT_H__