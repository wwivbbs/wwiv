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
#include "bbs/basic/message_base.h"
#include "bbs/basic/next/wwiv_bindings.h"

#include "common/context.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/msgapi/message.h"
#include "sdk/msgapi/message_area.h"
#include "sdk/msgapi/parsed_message.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "wwivbasic/interpreter.h"
#include "wwivbasic/object.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
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
  // The sub's messages, opened on first use and shared by the copies of this
  // area. The message area reads its files on each call, so it sees new
  // posts.
  mutable std::shared_ptr<sdk::msgapi::MessageArea> opened{};

  [[nodiscard]] const sdk::subboard_t& sub() const {
    // The sub list can change while a script holds an area.
    if (subnum < 0 || subnum >= msgs->subs().size()) {
      throw BasicError(fmt::format("message area '{}' no longer exists", key));
    }
    return msgs->subs().sub(subnum);
  }

  // The sub's messages, or nullptr if they can't be opened.
  [[nodiscard]] sdk::msgapi::MessageArea* area() const {
    if (!opened) {
      opened = msgs->open(subnum);
    }
    return opened.get();
  }

  [[nodiscard]] int count() const {
    const auto* a = area();
    return a ? a->number_of_messages() : 0;
  }
};

// A message header as scripts see it, after the BBS's rules for anonymous
// posts are applied.
struct ScriptHeader {
  int number{0};
  // The post's serial number in its sub (its qscan value). Unlike the
  // number, it doesn't change when earlier posts are deleted.
  uint32_t id{0};
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
  s.id = h.data().qscan;
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

// Stops early, with the headers read so far, once stopped() is true (the
// caller hung up); the interpreter then stops the script.
std::vector<ScriptHeader> read_headers(const ScriptArea& a, const std::function<bool()>& stopped) {
  std::vector<ScriptHeader> out;
  auto* area = a.area();
  if (!area) {
    return out;
  }
  const auto cosysop = a.msgs->is_cosysop(a.subnum);
  const auto read_anonymous = a.msgs->can_read_anonymous(a.subnum);
  const auto count = area->number_of_messages();
  for (auto i = 1; i <= count && !stopped(); i++) {
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
  auto* area = a.area();
  // An area that can't be opened has no messages, as COUNT says.
  const auto count = area ? area->number_of_messages() : 0;
  if (!area || number < 1 || number > count) {
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

// The number the post with serial id now has in area a, or 0 if it's gone
// or the user can't read it. number is where it was: deleting a post moves
// the later ones down, and serials increase with position, so it's there or
// before.
int find_message(const ScriptArea& a, uint32_t id, int number) {
  auto* area = a.area();
  if (!area || number < 1) {
    return 0;
  }
  const auto cosysop = a.msgs->is_cosysop(a.subnum);
  for (auto i = std::min(number, area->number_of_messages()); i >= 1; i--) {
    const auto h = area->ReadMessageHeader(i);
    if (!h) {
      continue;
    }
    const auto serial = h->data().qscan;
    if (serial == id) {
      return can_see(h.value(), cosysop) ? i : 0;
    }
    if (serial < id) {
      return 0;
    }
  }
  return 0;
}

// How the user's posts in sub may be anonymous: one of 0, anony_enable_anony,
// anony_enable_dear_abby, anony_force_anony or anony_real_name, worked out
// as the BBS's post() does (bbs/msgbase1.cpp).
uint8_t anonymous_setting(const sdk::subboard_t& sub, const MessageBase::Poster& p) {
  uint8_t a = sub.anony & 0x0f;
  if (a == 0 && p.sl_post_anony) {
    a = anony_enable_anony;
  }
  if (a == anony_enable_anony && p.restrict_anony) {
    a = 0;
  }
  if (!sub.nets.empty()) {
    a &= anony_real_name;
  }
  return a;
}

// What wwiv.msg's writing needs besides the area.
struct Writing {
  const sdk::Config* config;
  bool enabled;
};

// Why the user can't post in area a now, or "" if they can. The checks are
// the BBS's post()'s.
std::string post_error(const ScriptArea& a, const Writing& w) {
  const auto& sub = a.sub();
  if (!w.enabled) {
    return "posting from scripts isn't turned on on this BBS";
  }
  const auto p = a.msgs->poster(a.subnum);
  if (p.restrict_post) {
    return "you aren't allowed to post";
  }
  if (p.posts_today >= p.max_posts_today) {
    return "you've posted as many messages as you may today";
  }
  if (!a.msgs->can_post(a.subnum)) {
    return fmt::format("you can't post in '{}'", sub.name);
  }
  if (!sub.nets.empty() && p.restrict_net) {
    return fmt::format("you can't post in networked areas such as '{}'", sub.name);
  }
  if (core::File::freespace_for_path(w.config->msgsdir()) < 10) {
    return "there isn't enough disk space left";
  }
  if (!a.area()) {
    return fmt::format("'{}' can't be opened", sub.name);
  }
  return {};
}

// ^D starts WWIV's control lines (^D0PID: ...); ^Z ends message text.
constexpr char kControlD = 0x04;
constexpr char kControlZ = 0x1a;

// A script's text as WWIV message text: lines ending in CR LF, without the
// control characters that start WWIV's control lines (^D) or end the text
// (^Z). text is a string, or a list of lines.
std::string message_text(const Value& text) {
  std::string s;
  if (text.is_list()) {
    for (const auto& line : text.list("POST")->items) {
      s += line.toString();
      s += "\n";
    }
  } else {
    s = text.toString();
  }
  std::string out;
  for (size_t i = 0; i < s.size(); i++) {
    const auto c = s[i];
    if (c == '\r') {
      // CR LF or a lone CR ends a line, like LF.
      if (i + 1 < s.size() && s[i + 1] == '\n') {
        continue;
      }
      out += "\r\n";
    } else if (c == '\n') {
      out += "\r\n";
    } else if (c != kControlD && c != kControlZ) {
      out += c;
    }
  }
  if (!out.empty() && !out.ends_with("\r\n")) {
    out += "\r\n";
  }
  return out;
}

// Checks a one-line field of a post (its title or who it's to): at most 71
// characters (postrec's title holds 71), and no control characters, which
// would let a script end the line and start a control line (^D0...) of its
// own.
void check_field(const std::string& what, const std::string& s) {
  if (s.size() > 71) {
    throw BasicError(
        fmt::format("message_area.post: the {} is {} characters; the most is 71", what, s.size()));
  }
  for (const auto c : s) {
    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) {
      throw BasicError(fmt::format("message_area.post: the {} has a control character (code {})",
                                   what, static_cast<int>(static_cast<unsigned char>(c))));
    }
  }
}

// Saves a post the user wrote in area a, as the BBS's post() does, and returns
// its number. text follows the author and date lines, which the SDK adds.
int save_post(const ScriptArea& a, const std::string& fn, const std::string& title,
              const std::string& from, const std::string& to, uint8_t anony,
              const std::string& text, bool add_re_and_by_line) {
  const auto& sub = a.sub();
  const auto p = a.msgs->poster(a.subnum);
  auto* area = a.area();
  auto msg = area->CreateMessage();
  auto& h = msg.header();
  h.set_title(title);
  h.set_from(from);
  h.set_to(to);
  h.set_from_usernum(static_cast<uint16_t>(p.user_num));
  h.set_from_system(0);
  h.set_daten(core::daten_t_now());
  h.set_anony(anony);
  if (p.restrict_validate) {
    h.set_unvalidated(true);
  }
  msg.set_text(text);
  sdk::msgapi::MessageAreaOptions options;
  options.add_re_and_by_line = add_re_and_by_line;
  // The SDK sends it on the sub's first network only (#1782).
  options.send_post_to_network = !sub.nets.empty();
  if (!area->AddMessage(msg, options)) {
    throw BasicError(fmt::format("{}: the post couldn't be saved in '{}'", fn, sub.name));
  }
  a.msgs->posted(a.subnum, title);
  // Posts go at the end; a full area loses its oldest, not this one.
  return area->number_of_messages();
}

// POST(title, text[, to[, anonymous]]) in area a: posts as the BBS's post()
// does, and returns the new message's number.
int post_message(const ScriptArea& a, const Writing& w, const std::vector<Value>& args) {
  if (args.size() < 2 || args.size() > 4) {
    throw wwivbasic::ArgumentError(fmt::format("expected 2 to 4 arguments, got {}", args.size()));
  }
  const auto title = StringTrim(args.at(0).toString());
  const auto to = args.size() > 2 ? StringTrim(args.at(2).toString()) : std::string();
  const auto anonymous = args.size() > 3 && args.at(3).toBool();
  const auto& sub = a.sub();
  if (const auto e = post_error(a, w); !e.empty()) {
    throw BasicError(fmt::format("message_area.post: {}", e));
  }
  if (title.empty()) {
    throw BasicError("message_area.post: the title is empty");
  }
  check_field("title", title);
  check_field("name it's to", to);
  const auto text = message_text(args.at(1));
  if (text.empty()) {
    throw BasicError("message_area.post: the text is empty");
  }

  const auto p = a.msgs->poster(a.subnum);
  auto from = p.name;
  uint8_t anony = 0;
  switch (anonymous_setting(sub, p)) {
  case anony_enable_anony:
    anony = anonymous ? anony_sender : 0;
    break;
  case anony_enable_dear_abby:
    // TRUE posts as "Abby"; the BBS's editor also offers "Problemed Person".
    anony = anonymous ? anony_sender_da : 0;
    break;
  case anony_force_anony:
    anony = anony_sender;
    break;
  case anony_real_name:
    from = p.real_name;
    [[fallthrough]];
  default:
    if (anonymous) {
      throw BasicError(
          fmt::format("message_area.post: posts in '{}' can't be anonymous", sub.name));
    }
    break;
  }

  // As inmsg: the program that wrote it, then the text. RE: and BY: lines
  // only for a post to someone, as inmsg adds them.
  return save_post(a, "message_area.post", title, from, to, anony,
                   fmt::format("{}0PID: WWIV {}\r\n{}", kControlD, core::full_version(), text),
                   !to.empty() && !iequals(to, "All"));
}

// WRITE_POST([title[, to]]) and REPLY(h) in area a: the user writes the post
// in the BBS's message editor, which asks for the title if it isn't given
// (offering "Re: " + irt), and whether to post anonymously where the user
// may choose. Returns the new message's number, or 0 if the user aborted.
int edit_and_post(const ScriptArea& a, const Writing& w, const std::string& fn,
                  const std::string& title, const std::string& irt, const std::string& to) {
  if (const auto e = post_error(a, w); !e.empty()) {
    throw BasicError(fmt::format("message_area.{}: {}", fn, e));
  }
  check_field("title", title);
  check_field("name it's to", to);
  const auto p = a.msgs->poster(a.subnum);
  const auto edited = a.msgs->edit_post(a.subnum, title, irt, to, anonymous_setting(a.sub(), p));
  if (!edited) {
    return 0;
  }
  // The editor's text already has the RE:/BY: (or FidoAddr) and PID lines.
  // The editor's "to" is in those lines; giving it to the SDK too would add
  // a second FidoAddr line on FTN subs.
  return save_post(a, fmt::format("message_area.{}", fn), edited->title, edited->from, "",
                   edited->anony, edited->text, false);
}

// DELETE_MESSAGE(h) in area a: deletes the post h (a header or message from
// a) if it's the user's own or the user is a co-sysop of the area. Returns
// FALSE if it's already gone.
bool delete_message(const ScriptArea& a, const Writing& w, uint32_t id, int number) {
  const auto& sub = a.sub();
  if (!w.enabled) {
    throw BasicError(
        "message_area.delete_message: deleting from scripts isn't turned on on this BBS");
  }
  const auto n = find_message(a, id, number);
  if (n == 0) {
    return false;
  }
  auto* area = a.area();
  const auto h = area->ReadMessageHeader(n);
  if (!h) {
    return false;
  }
  const auto user_num = a.msgs->poster(a.subnum).user_num;
  const auto own = h->from_system() == 0 && h->from_usernum() == user_num;
  if (!own && !a.msgs->is_cosysop(a.subnum)) {
    throw BasicError(fmt::format(
        "message_area.delete_message: you can only delete your own posts in '{}'", sub.name));
  }
  if (!area->DeleteMessage(n)) {
    throw BasicError(fmt::format(
        "message_area.delete_message: message {} in '{}' couldn't be deleted", n, sub.name));
  }
  a.msgs->deleted(a.subnum, h->title(), h->from_usernum(), h->daten());
  return true;
}

// Adds the header properties to the binding for T (ScriptHeader or
// ScriptMessage).
template <class T> void add_header_properties(wwivbasic::ClassBinding<T>& c) {
  c.property("number", [](const T& h) { return h.number; })
      .property("title", [](const T& h) { return h.title; })
      .property("from", [](const T& h) { return h.from; })
      .property("to", [](const T& h) { return h.to; })
      .property("id", [](const T& h) { return Value::number(static_cast<double>(h.id)); })
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
  // True once the caller hangs up.
  std::function<bool()> hung_up = [ctx = b.ctx]() {
    return ctx && ctx->session_context().hangup();
  };
  const Writing w{&b.config, b.config.script_package_msg_write_enabled()};
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
              [&header_class, hung_up](const ScriptArea& a) {
                std::vector<Value> items;
                for (auto& h : read_headers(a, hung_up)) {
                  items.push_back(header_class.wrap(std::move(h)));
                }
                return Value::make_list(std::move(items));
              })
      // MESSAGE(n) reads message n (from 1 to COUNT).
      .method("message",
              [&message_class](const ScriptArea& a, int number) {
                return message_class.wrap(read_message(a, number));
              })
      // FIND(h) is the number the message h (a header or message from this
      // area) has now, or 0 if it's gone or can't be read. Numbers shift when
      // earlier posts are deleted; use it before MESSAGE to read the post h
      // was rather than whatever now has its number.
      .method("find",
              [&header_class, &message_class](const ScriptArea& a, Value v) {
                if (const auto h = header_class.unwrap(v)) {
                  return find_message(a, h->id, h->number);
                }
                if (const auto msg = message_class.unwrap(v)) {
                  return find_message(a, msg->id, msg->number);
                }
                throw BasicError("FIND: expected a message_header or message");
              })
      // CAN_POST is TRUE if the user may post in the area now.
      .property("can_post", [w](const ScriptArea& a) { return post_error(a, w).empty(); })
      // ANONYMOUS is how the user's posts in the area may be anonymous: "no",
      // "allowed" (POST's anonymous argument chooses) or "forced".
      .property("anonymous",
                [](const ScriptArea& a) -> std::string {
                  switch (anonymous_setting(a.sub(), a.msgs->poster(a.subnum))) {
                  case anony_enable_anony:
                  case anony_enable_dear_abby:
                    return "allowed";
                  case anony_force_anony:
                    return "forced";
                  default:
                    return "no";
                  }
                })
      // POST(title, text[, to[, anonymous]]) posts text (a string, or a list
      // of lines) and returns the new message's number.
      .methodl("post",
               [w](ScriptArea& a, const std::vector<Value>& args) {
                 return Value(post_message(a, w, args));
               })
      // WRITE_POST([title[, to]]) lets the user write a post in their message
      // editor, as the BBS's post command does, and returns its number, or 0
      // if they aborted. Without a title the editor asks for one.
      .methodl("write_post",
               [w](ScriptArea& a, const std::vector<Value>& args) {
                 if (args.size() > 2) {
                   throw wwivbasic::ArgumentError(
                       fmt::format("expected 0 to 2 arguments, got {}", args.size()));
                 }
                 const auto title =
                     args.empty() ? std::string() : StringTrim(args.at(0).toString());
                 const auto to =
                     args.size() > 1 ? StringTrim(args.at(1).toString()) : std::string();
                 return Value(edit_and_post(a, w, "write_post", title, "", to));
               })
      // REPLY(h) lets the user reply to the post h (a header or message from
      // any area) in their message editor: to its author, with "Re: " and its
      // title offered as the title. Returns the reply's number, or 0.
      .method("reply",
              [w, &header_class, &message_class](ScriptArea& a, Value v) {
                std::string title;
                std::string from;
                if (const auto h = header_class.unwrap(v)) {
                  title = h->title;
                  from = h->from;
                } else if (const auto msg = message_class.unwrap(v)) {
                  title = msg->title;
                  from = msg->from;
                } else {
                  throw BasicError("message_area.reply: expected a message_header or message");
                }
                return edit_and_post(a, w, "reply", "", title, from);
              })
      // DELETE_MESSAGE(h) deletes the post h (a header or message from this
      // area): the user's own, or any if they're a co-sysop of the area.
      // FALSE if it was already gone.
      .method("delete_message", [w, &header_class, &message_class](ScriptArea& a, Value v) {
        if (const auto h = header_class.unwrap(v)) {
          return delete_message(a, w, h->id, h->number);
        }
        if (const auto msg = message_class.unwrap(v)) {
          return delete_message(a, w, msg->id, msg->number);
        }
        throw BasicError("message_area.delete_message: expected a message_header or message");
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
    const auto u = msgs->current_user_sub();
    if (!u) {
      throw BasicError("CURRENT_AREA: the user isn't in a message area");
    }
    return make_area(*u);
  });
}

} // namespace wwiv::bbs::basic
