/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "bbs/basic/next/message_base.h"
#include "bbs/basic/next/wwiv_bindings.h"

#include "core/strings.h"
#include "fmt/format.h"
#include "sdk/msgapi/message.h"
#include "sdk/msgapi/message_area.h"
#include "sdk/msgapi/parsed_message.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/object.h"

#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace wwiv::strings;

namespace wwiv::bbs::basic {

using wwivbasic::BasicError;
using wwivbasic::Value;

namespace {

// A message area the user can read (one of MessageBase::user_subs()).
struct ScriptArea {
  MessageBase* msgs;
  int subnum;
  std::string key;

  [[nodiscard]] const sdk::subboard_t& sub() const { return msgs->subs().sub(subnum); }

  [[nodiscard]] int count() const {
    const auto area = msgs->open(subnum);
    return area ? area->number_of_messages() : 0;
  }
};

// A message header as scripts see it, after the BBS's rules for anonymous
// posts are applied.
struct ScriptHeader {
  int number{0};
  std::string title;
  std::string from;
  std::string to;
  // When it was posted (a time_t), or 0 if that is hidden.
  daten_t date{0};
  bool anonymous{false};
};

// A message: its header and the lines of its text.
struct ScriptMessage : ScriptHeader {
  std::vector<std::string> lines;
};

// Applies the anonymous post rules of the BBS's reader (UpdateHeaderInformation
// in bbs/read_message.cpp), except that a user who may see the sender gets
// the plain name rather than "<<< name >>>"; `anonymous` says it was posted
// anonymously.
ScriptHeader to_script_header(int number, const sdk::msgapi::MessageHeader& h,
                              bool read_anonymous) {
  ScriptHeader s;
  s.number = number;
  s.title = h.title();
  s.from = h.from();
  s.to = h.to();
  s.date = h.daten();
  switch (h.anony() & 0x0f) {
  case anony_sender:
    s.anonymous = true;
    if (!read_anonymous) {
      s.from = ">UNKNOWN<";
    }
    break;
  case anony_sender_da:
  case anony_sender_pp:
    s.anonymous = true;
    // The BBS hides the date of these from everyone.
    s.date = 0;
    if (!read_anonymous) {
      s.from = (h.anony() & 0x0f) == anony_sender_da ? "Abby" : "Problemed Person";
    }
    break;
  default:
    break;
  }
  return s;
}

// The BBS's reader skips unvalidated and deleted posts unless the user is a
// co-sysop of the sub (read_post in bbs/read_message.cpp).
bool can_see(const sdk::msgapi::MessageHeader& h, bool cosysop) {
  return cosysop || !(h.unvalidated() || h.deleted());
}

// The lines of a message's text, without WWIV's control lines (^D0) and
// without wrapping long lines.
std::vector<std::string> text_lines(const std::string& text) {
  sdk::msgapi::parsed_message_lines_style_t style{};
  style.ctrl_lines = sdk::msgapi::control_lines_t::no_control_lines;
  style.line_length = std::numeric_limits<int>::max();
  style.add_wrapping_marker = false;
  return sdk::msgapi::WWIVParsedMessageText(text).to_lines(style);
}

std::vector<ScriptHeader> read_headers(const ScriptArea& a) {
  std::vector<ScriptHeader> out;
  const auto area = a.msgs->open(a.subnum);
  if (!area) {
    return out;
  }
  const auto cosysop = a.msgs->is_cosysop(a.subnum);
  const auto read_anonymous = a.msgs->can_read_anonymous(a.subnum);
  const auto count = area->number_of_messages();
  for (auto i = 1; i <= count; i++) {
    const auto h = area->ReadMessageHeader(i);
    if (!h || !can_see(h.value(), cosysop)) {
      continue;
    }
    out.push_back(to_script_header(i, h.value(), read_anonymous));
  }
  return out;
}

ScriptMessage read_message(const ScriptArea& a, int number) {
  const auto& name = a.sub().name;
  const auto area = a.msgs->open(a.subnum);
  if (!area) {
    throw BasicError(fmt::format("MESSAGE: unable to open the message area '{}'", name));
  }
  const auto count = area->number_of_messages();
  if (number < 1 || number > count) {
    throw BasicError(
        fmt::format("MESSAGE: there is no message {} in '{}' (it has {})", number, name, count));
  }
  const auto m = area->ReadMessage(number);
  if (!m || !can_see(m->header(), a.msgs->is_cosysop(a.subnum))) {
    throw BasicError(fmt::format("MESSAGE: message {} in '{}' can't be read", number, name));
  }
  ScriptMessage out;
  static_cast<ScriptHeader&>(out) =
      to_script_header(number, m->header(), a.msgs->can_read_anonymous(a.subnum));
  out.lines = text_lines(m->text().string());
  return out;
}

// Adds the header properties to the binding for T (ScriptHeader or
// ScriptMessage).
template <class T> void add_header_properties(wwivbasic::ClassBinding<T>& c) {
  c.property("number", [](const T& h) { return h.number; })
      .property("title", [](const T& h) { return h.title; })
      .property("from", [](const T& h) { return h.from; })
      .property("to", [](const T& h) { return h.to; })
      .property("date", [](const T& h) { return h.date; })
      .property("anonymous", [](const T& h) { return h.anonymous; });
}

} // namespace

// The wwiv.msg module (new engine only): read-only access to the message
// areas the user can read, with the same rights as the BBS's reader.
void RegisterWwivMsg(wwivbasic::Interpreter& interp, const WwivBindings& b) {
  if (!b.msgs) {
    return;
  }
  auto* msgs = b.msgs;
  auto& m = interp.add_module("wwiv.msg");
  auto& header_class = m.class_<ScriptHeader>("message_header");
  auto& message_class = m.class_<ScriptMessage>("message");
  auto& area_class = m.class_<ScriptArea>("message_area");

  add_header_properties(header_class);
  add_header_properties(message_class);
  message_class.property("text", [](const ScriptMessage& msg) {
    return JoinStrings(msg.lines, "\r\n");
  });
  message_class.method("lines", [](const ScriptMessage& msg) { return msg.lines; });

  area_class.property("key", [](const ScriptArea& a) { return a.key; })
      .property("name", [](const ScriptArea& a) { return a.sub().name; })
      .property("description", [](const ScriptArea& a) { return a.sub().desc; })
      .property("count", [](const ScriptArea& a) { return a.count(); })
      // HEADERS() lists the headers of the messages the user can read.
      .method("headers",
              [&header_class](const ScriptArea& a) {
                std::vector<Value> items;
                for (auto& h : read_headers(a)) {
                  items.push_back(header_class.wrap(std::move(h)));
                }
                return Value::make_list(std::move(items));
              })
      // MESSAGE(n) reads message n (from 1 to COUNT).
      .method("message", [&message_class](const ScriptArea& a, int number) {
        return message_class.wrap(read_message(a, number));
      });

  auto make_area = [msgs, &area_class](const MessageBase::UserSub& u) {
    return area_class.wrap(ScriptArea{msgs, u.subnum, u.key});
  };

  m.native_function("MODULE_NAME", []() { return std::string("wwiv.msg"); });

  // AREAS() lists the message areas the user can read, as the BBS lists them.
  m.native_function("AREAS", [msgs, make_area]() {
    std::vector<Value> items;
    for (const auto& u : msgs->user_subs()) {
      items.push_back(make_area(u));
    }
    return Value::make_list(std::move(items));
  });

  // AREA(key) is the area the user chooses with key.
  m.native_function("AREA", [msgs, make_area](std::string key) {
    for (const auto& u : msgs->user_subs()) {
      if (iequals(u.key, key)) {
        return make_area(u);
      }
    }
    throw BasicError(fmt::format("AREA: there is no message area '{}'", key));
  });

  // CURRENT_AREA() is the area the user is in.
  m.native_function("CURRENT_AREA", [msgs, make_area]() {
    const auto subs = msgs->user_subs();
    const auto n = msgs->current_user_sub();
    if (!n || *n < 0 || *n >= static_cast<int>(subs.size())) {
      throw BasicError("CURRENT_AREA: the user isn't in a message area");
    }
    return make_area(subs[*n]);
  });
}

} // namespace wwiv::bbs::basic
