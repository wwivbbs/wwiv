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
#include "gtest/gtest.h"

#include "bbs/bbs_helper.h"
#include "bbs/message_file.h"
#include "bbs/read_message.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/parsed_message.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"

#include <limits>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

namespace {

// The lines the BBS's reader shows: split_wwiv_message in read_message.cpp,
// with ^D0 lines hidden (controlcodes off) and no wrapping.
std::vector<std::string> shown_lines(const std::string& text) {
  WWIVParsedMessageText pmt(text);
  parsed_message_lines_style_t style{};
  style.ctrl_lines = control_lines_t::control_lines;
  style.add_wrapping_marker = false;
  style.line_length = std::numeric_limits<int>::max();
  std::vector<std::string> out;
  for (const auto& line : pmt.to_lines(style)) {
    if (!line.empty() && line.front() == '\x04' && line.size() > 1 && line[1] == '0') {
      continue;
    }
    out.push_back(line);
  }
  return out;
}

std::string show(const std::vector<std::string>& lines) {
  std::string s;
  for (const auto& l : lines) {
    std::string e;
    for (const auto c : l) {
      if (static_cast<unsigned char>(c) < 32) {
        e += fmt::format("^{}", static_cast<char>(c + '@'));
      } else {
        e += c;
      }
    }
    s += "[" + e + "]\n";
  }
  return s;
}

} // namespace

// Characterization test: sdk/msgapi must read a post's text as the BBS's
// reader (read_type2_message and split_wwiv_message in read_message.cpp)
// shows it, so that the BBS can move onto sdk/msgapi and scripts (wwiv.msg)
// see what callers see. The same raw type-2 text is read both ways and the
// lines the reader would show are compared.
class LegacyCompareTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    ASSERT_TRUE(helper.files().Mkdir("msgs"));
    a()->init_status_and_instances_for_test();
    MessageApiOptions options;
    options.overflow_strategy = OverflowStrategy::delete_none;
    api = std::make_unique<WWIVMessageApi>(options, helper.config(),
                                           std::vector<net::Network>{}, new NullLastReadImpl());
    sub.filename = "cmp";
    sub.storage_type = 2;
    ASSERT_TRUE(api->Create(sub, 0));
  }

  // Lines from the BBS's reader (via email, which needs no current sub).
  std::vector<std::string> Legacy(const std::string& raw) {
    messagerec m{};
    m.storage_type = 2;
    savefile(raw, &m, "email");
    // anony_sender + readit skips the network origin lookup.
    auto d = read_type2_message(&m, anony_sender, true, "email", 0, 0);
    EXPECT_TRUE(d);
    return d ? shown_lines(d->message_text) : std::vector<std::string>{};
  }

  // Lines from sdk/msgapi: the same raw text as post 1 of sub "cmp".
  std::vector<std::string> Sdk(const std::string& raw) {
    {
      auto area = api->Open(sub, 0);
      auto msg = area->CreateMessage();
      msg.header().set_title("t");
      msg.set_text("x");
      EXPECT_TRUE(area->AddMessage(msg, {}));
    }
    messagerec m{};
    m.storage_type = 2;
    savefile(raw, &m, "cmp");
    int n;
    {
      auto area = api->Open(sub, 0);
      n = area->number_of_messages();
    }
    DataFile<postrec> f(FilePath(helper.config().datadir(), "cmp.sub"),
                        File::modeBinary | File::modeReadWrite);
    postrec p{};
    EXPECT_TRUE(f.Read(n, &p));
    p.msg = m;
    EXPECT_TRUE(f.Write(n, &p));
    f.Close();
    auto area = api->Open(sub, 0);
    const auto r = area->ReadMessage(n);
    EXPECT_TRUE(r);
    return r ? shown_lines(r->text().string()) : std::vector<std::string>{};
  }

  BbsHelper helper{};
  std::unique_ptr<WWIVMessageApi> api;
  subboard_t sub{};
};

TEST_F(LegacyCompareTest, Same) {
  const std::vector<std::pair<const char*, const char*>> cases{
        std::make_pair("simple", "Hello\r\nWorld\r\n\x1a"),
        std::make_pair("paragraphs", "One\r\n\r\nTwo\r\n\r\n\r\nThree\r\n\x1a"),
        std::make_pair("leading_blank", "\r\n\r\nHello\r\n\x1a"),
        std::make_pair("indented", "  two\r\n    four\r\n\ttab\r\n\x1a"),
        std::make_pair("trailing_blank", "Hello\r\n\r\n\r\n\x1a"),
        std::make_pair("trailing_spaces", "Hello   \r\nWorld \r\n\x1a"),
        std::make_pair("re_by", "RE: Subject\r\nBY: someone\r\nHello\r\n\x1a"),
        std::make_pair("ctrl_top", "\x04" "0PID: x\r\n\x04" "0MSGID: y\r\nHello\r\n\x1a"),
        std::make_pair("ctrl_middle", "Hello\r\n\x04" "0Kludge\r\nWorld\r\n\x1a"),
        std::make_pair("tagline", "Hello\r\n\x04" "1tag line\r\n\x04" "2more\r\n\x1a"),
        std::make_pair("cz_middle", "Hello\r\n\x1a" "junk\r\n"),
        std::make_pair("no_final_newline", "Hello\r\nWorld\x1a"),
        std::make_pair("no_cz", "Hello\r\nWorld\r\n"),
        std::make_pair("cr_only", "Hello\rWorld\r\x1a"),
        std::make_pair("soft_wrap", "This line was\x01\r\nwrapped\r\n\x1a"),
        std::make_pair("pipe_colors", "|#1Hello |09World\r\n\x1a"),
        std::make_pair("blank_after_kludge", "\x04" "0PID: x\r\n\r\nHello\r\n\x1a"),
        std::make_pair("blank_around_re", "\r\nRE: Subject\r\n\r\nHello\r\n\x1a"),
        std::make_pair("first_line_indented", "   Hello\r\n\x1a"),
        std::make_pair("ansi", "\x1b[1;31mRed\x1b[0m\r\n\x1a"),
        std::make_pair("only_cz", "\x1a"),
        std::make_pair("blank_lines_only", "\r\n\r\n\x1a"),
        std::make_pair("quote", " RF> quoted\r\n RF> more\r\n\r\nReply\r\n\x1a")};
  for (const auto& [name, body] : cases) {
    SCOPED_TRACE(name);
    const auto raw = StrCat("rushfan #1\r\nMon Nov 23 2026\r\n", body);
    auto legacy = Legacy(raw);
    const auto sdk = Sdk(raw);
    // The one intended difference: the SDK reads the RE: and BY: lines at the
    // top into the header (in_reply_to and to), so they aren't in the text.
    while (!legacy.empty() &&
           (legacy.front().starts_with("RE:") || legacy.front().starts_with("BY:"))) {
      legacy.erase(legacy.begin());
    }
    EXPECT_EQ(show(legacy), show(sdk));
  }
}
