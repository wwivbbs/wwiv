/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*           Copyright (C)2020-2021, WWIV Software Services               */
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
#include "sdk/files/diz.h"

#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include <utility>

static const char* invalid_chars = "Ú¿ÀÙÄ³Ã´ÁÂÉ»È¼ÍºÌ¹ÊËÕ¸Ô¾Í³ÆµÏÑÖ·Ó½ÄºÇ¶ÐÒÅÎØ×°±²ÛßÜÝÞ";

wwiv::sdk::files::Diz::Diz(std::string description, std::string extended_description)
    : description_(std::move(description)), extended_description_(std::move(extended_description)) {}

std::string wwiv::sdk::files::Diz::description() const noexcept { return description_; }

std::string wwiv::sdk::files::Diz::extended_description() const noexcept {
  return extended_description_;
}

wwiv::sdk::files::DizParser::DizParser(bool firstline_as_desc)
    : firstline_as_desc_(firstline_as_desc) {}


static bool valid_desc(const std::string& description) {
  // I don't think this function is really doing what it should
  // be doing, but am not sure what it should be doing instead.
  for (const auto& c : description) {
    if (c > '@' && c < '{') {
      return true;
    }
  }
  return false;
}

static std::string fixup_diz_string(const std::string& orig) {
  auto s{orig};
  std::replace(s.begin(), s.end(), '\x0c', ' ');
  std::replace(s.begin(), s.end(), '\x1a', ' ');
  return s;
}

std::optional<wwiv::sdk::files::Diz> wwiv::sdk::files::DizParser::parse(
    const std::filesystem::path& path) const {
  if (!core::File::Exists(path)) {
    return std::nullopt;
  }

  std::string description;

  TextFile file(path, "rt");
  auto lines = file.ReadFileIntoVector();
  if (lines.empty()) {
    return std::nullopt;
  }
  auto iter = std::begin(lines);
  if (firstline_as_desc_) {
    auto ss = *iter;
    if (!ss.empty()) {
      for (auto& s : ss) {
        if (strchr(invalid_chars, s) != nullptr && s != 26) {
          s = ' ';
        }
      }
      if (!valid_desc(ss)) {
        do {
          ++iter;
          ss = *iter;
        }
        while (!valid_desc(ss) && iter != std::end(lines));
      }
    }
    if (!ss.empty() && ss.back() == '\r') {
      ss.pop_back();
    }

    description = strings::StringTrim(ss);
    // Only bail here if we have nothing.
    if (description.empty()) {
      return std::nullopt;
    }

    ++iter;
  } else {
    iter = std::begin(lines);
  }
  std::string ext;
  for (; iter != std::end(lines); ++iter) {
    ext.append(fixup_diz_string(*iter));
    ext.append("\n");
  }

  if (description.empty() && ext.empty()) {
    return std::nullopt;
  }

  return {Diz(description, ext)};
}
