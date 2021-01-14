/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
#include "core/cp437.h"

#include "core/stl.h"
#include <clocale>
#include <cstdint>

#ifdef _WIN32
#include "core/wwiv_windows.h"
#endif

//////////////////////////////////////////////////////////////////////////////
//
// Data from ftp://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP437.TXT
//
//
// Main table generated as:
// cat CP437.TXT | grep -v "^#" | awk '{print  $2 ", //  [" $1 "] " $3} '

namespace wwiv::core {

using namespace wwiv::stl;

static wchar_t dos_to_utf8_[] =
{
0x0000, //  [0x00] #NULL
0x0001, //  [0x01] #START
0x0002, //  [0x02] #START
0x0003, //  [0x03] #END
0x0004, //  [0x04] #END
0x0005, //  [0x05] #ENQUIRY
0x0006, //  [0x06] #ACKNOWLEDGE
0x0007, //  [0x07] #BELL
0x0008, //  [0x08] #BACKSPACE
0x0009, //  [0x09] #HORIZONTAL
0x000a, //  [0x0a] #LINE
0x000b, //  [0x0b] #VERTICAL
0x000c, //  [0x0c] #FORM
0x000d, //  [0x0d] #CARRIAGE
0x000e, //  [0x0e] #SHIFT
0x000f, //  [0x0f] #SHIFT
0x0010, //  [0x10] #DATA
0x0011, //  [0x11] #DEVICE
0x0012, //  [0x12] #DEVICE
0x0013, //  [0x13] #DEVICE
0x0014, //  [0x14] #DEVICE
0x0015, //  [0x15] #NEGATIVE
0x0016, //  [0x16] #SYNCHRONOUS
0x0017, //  [0x17] #END
0x0018, //  [0x18] #CANCEL
0x0019, //  [0x19] #END
0x001a, //  [0x1a] #SUBSTITUTE
0x001b, //  [0x1b] #ESCAPE
0x001c, //  [0x1c] #FILE
0x001d, //  [0x1d] #GROUP
0x001e, //  [0x1e] #RECORD
0x001f, //  [0x1f] #UNIT
0x0020, //  [0x20] #SPACE
0x0021, //  [0x21] #EXCLAMATION
0x0022, //  [0x22] #QUOTATION
0x0023, //  [0x23] #NUMBER
0x0024, //  [0x24] #DOLLAR
0x0025, //  [0x25] #PERCENT
0x0026, //  [0x26] #AMPERSAND
0x0027, //  [0x27] #APOSTROPHE
0x0028, //  [0x28] #LEFT
0x0029, //  [0x29] #RIGHT
0x002a, //  [0x2a] #ASTERISK
0x002b, //  [0x2b] #PLUS
0x002c, //  [0x2c] #COMMA
0x002d, //  [0x2d] #HYPHEN-MINUS
0x002e, //  [0x2e] #FULL
0x002f, //  [0x2f] #SOLIDUS
0x0030, //  [0x30] #DIGIT
0x0031, //  [0x31] #DIGIT
0x0032, //  [0x32] #DIGIT
0x0033, //  [0x33] #DIGIT
0x0034, //  [0x34] #DIGIT
0x0035, //  [0x35] #DIGIT
0x0036, //  [0x36] #DIGIT
0x0037, //  [0x37] #DIGIT
0x0038, //  [0x38] #DIGIT
0x0039, //  [0x39] #DIGIT
0x003a, //  [0x3a] #COLON
0x003b, //  [0x3b] #SEMICOLON
0x003c, //  [0x3c] #LESS-THAN
0x003d, //  [0x3d] #EQUALS
0x003e, //  [0x3e] #GREATER-THAN
0x003f, //  [0x3f] #QUESTION
0x0040, //  [0x40] #COMMERCIAL
0x0041, //  [0x41] #LATIN
0x0042, //  [0x42] #LATIN
0x0043, //  [0x43] #LATIN
0x0044, //  [0x44] #LATIN
0x0045, //  [0x45] #LATIN
0x0046, //  [0x46] #LATIN
0x0047, //  [0x47] #LATIN
0x0048, //  [0x48] #LATIN
0x0049, //  [0x49] #LATIN
0x004a, //  [0x4a] #LATIN
0x004b, //  [0x4b] #LATIN
0x004c, //  [0x4c] #LATIN
0x004d, //  [0x4d] #LATIN
0x004e, //  [0x4e] #LATIN
0x004f, //  [0x4f] #LATIN
0x0050, //  [0x50] #LATIN
0x0051, //  [0x51] #LATIN
0x0052, //  [0x52] #LATIN
0x0053, //  [0x53] #LATIN
0x0054, //  [0x54] #LATIN
0x0055, //  [0x55] #LATIN
0x0056, //  [0x56] #LATIN
0x0057, //  [0x57] #LATIN
0x0058, //  [0x58] #LATIN
0x0059, //  [0x59] #LATIN
0x005a, //  [0x5a] #LATIN
0x005b, //  [0x5b] #LEFT
0x005c, //  [0x5c] #REVERSE
0x005d, //  [0x5d] #RIGHT
0x005e, //  [0x5e] #CIRCUMFLEX
0x005f, //  [0x5f] #LOW
0x0060, //  [0x60] #GRAVE
0x0061, //  [0x61] #LATIN
0x0062, //  [0x62] #LATIN
0x0063, //  [0x63] #LATIN
0x0064, //  [0x64] #LATIN
0x0065, //  [0x65] #LATIN
0x0066, //  [0x66] #LATIN
0x0067, //  [0x67] #LATIN
0x0068, //  [0x68] #LATIN
0x0069, //  [0x69] #LATIN
0x006a, //  [0x6a] #LATIN
0x006b, //  [0x6b] #LATIN
0x006c, //  [0x6c] #LATIN
0x006d, //  [0x6d] #LATIN
0x006e, //  [0x6e] #LATIN
0x006f, //  [0x6f] #LATIN
0x0070, //  [0x70] #LATIN
0x0071, //  [0x71] #LATIN
0x0072, //  [0x72] #LATIN
0x0073, //  [0x73] #LATIN
0x0074, //  [0x74] #LATIN
0x0075, //  [0x75] #LATIN
0x0076, //  [0x76] #LATIN
0x0077, //  [0x77] #LATIN
0x0078, //  [0x78] #LATIN
0x0079, //  [0x79] #LATIN
0x007a, //  [0x7a] #LATIN
0x007b, //  [0x7b] #LEFT
0x007c, //  [0x7c] #VERTICAL
0x007d, //  [0x7d] #RIGHT
0x007e, //  [0x7e] #TILDE
0x007f, //  [0x7f] #DELETE
0x00c7, //  [0x80] #LATIN
0x00fc, //  [0x81] #LATIN
0x00e9, //  [0x82] #LATIN
0x00e2, //  [0x83] #LATIN
0x00e4, //  [0x84] #LATIN
0x00e0, //  [0x85] #LATIN
0x00e5, //  [0x86] #LATIN
0x00e7, //  [0x87] #LATIN
0x00ea, //  [0x88] #LATIN
0x00eb, //  [0x89] #LATIN
0x00e8, //  [0x8a] #LATIN
0x00ef, //  [0x8b] #LATIN
0x00ee, //  [0x8c] #LATIN
0x00ec, //  [0x8d] #LATIN
0x00c4, //  [0x8e] #LATIN
0x00c5, //  [0x8f] #LATIN
0x00c9, //  [0x90] #LATIN
0x00e6, //  [0x91] #LATIN
0x00c6, //  [0x92] #LATIN
0x00f4, //  [0x93] #LATIN
0x00f6, //  [0x94] #LATIN
0x00f2, //  [0x95] #LATIN
0x00fb, //  [0x96] #LATIN
0x00f9, //  [0x97] #LATIN
0x00ff, //  [0x98] #LATIN
0x00d6, //  [0x99] #LATIN
0x00dc, //  [0x9a] #LATIN
0x00a2, //  [0x9b] #CENT
0x00a3, //  [0x9c] #POUND
0x00a5, //  [0x9d] #YEN
0x20a7, //  [0x9e] #PESETA
0x0192, //  [0x9f] #LATIN
0x00e1, //  [0xa0] #LATIN
0x00ed, //  [0xa1] #LATIN
0x00f3, //  [0xa2] #LATIN
0x00fa, //  [0xa3] #LATIN
0x00f1, //  [0xa4] #LATIN
0x00d1, //  [0xa5] #LATIN
0x00aa, //  [0xa6] #FEMININE
0x00ba, //  [0xa7] #MASCULINE
0x00bf, //  [0xa8] #INVERTED
0x2310, //  [0xa9] #REVERSED
0x00ac, //  [0xaa] #NOT
0x00bd, //  [0xab] #VULGAR
0x00bc, //  [0xac] #VULGAR
0x00a1, //  [0xad] #INVERTED
0x00ab, //  [0xae] #LEFT-POINTING
0x00bb, //  [0xaf] #RIGHT-POINTING
0x2591, //  [0xb0] #LIGHT
0x2592, //  [0xb1] #MEDIUM
0x2593, //  [0xb2] #DARK
0x2502, //  [0xb3] #BOX
0x2524, //  [0xb4] #BOX
0x2561, //  [0xb5] #BOX
0x2562, //  [0xb6] #BOX
0x2556, //  [0xb7] #BOX
0x2555, //  [0xb8] #BOX
0x2563, //  [0xb9] #BOX
0x2551, //  [0xba] #BOX
0x2557, //  [0xbb] #BOX
0x255d, //  [0xbc] #BOX
0x255c, //  [0xbd] #BOX
0x255b, //  [0xbe] #BOX
0x2510, //  [0xbf] #BOX
0x2514, //  [0xc0] #BOX
0x2534, //  [0xc1] #BOX
0x252c, //  [0xc2] #BOX
0x251c, //  [0xc3] #BOX
0x2500, //  [0xc4] #BOX
0x253c, //  [0xc5] #BOX
0x255e, //  [0xc6] #BOX
0x255f, //  [0xc7] #BOX
0x255a, //  [0xc8] #BOX
0x2554, //  [0xc9] #BOX
0x2569, //  [0xca] #BOX
0x2566, //  [0xcb] #BOX
0x2560, //  [0xcc] #BOX
0x2550, //  [0xcd] #BOX
0x256c, //  [0xce] #BOX
0x2567, //  [0xcf] #BOX
0x2568, //  [0xd0] #BOX
0x2564, //  [0xd1] #BOX
0x2565, //  [0xd2] #BOX
0x2559, //  [0xd3] #BOX
0x2558, //  [0xd4] #BOX
0x2552, //  [0xd5] #BOX
0x2553, //  [0xd6] #BOX
0x256b, //  [0xd7] #BOX
0x256a, //  [0xd8] #BOX
0x2518, //  [0xd9] #BOX
0x250c, //  [0xda] #BOX
0x2588, //  [0xdb] #FULL
0x2584, //  [0xdc] #LOWER
0x258c, //  [0xdd] #LEFT
0x2590, //  [0xde] #RIGHT
0x2580, //  [0xdf] #UPPER
0x03b1, //  [0xe0] #GREEK
0x00df, //  [0xe1] #LATIN
0x0393, //  [0xe2] #GREEK
0x03c0, //  [0xe3] #GREEK
0x03a3, //  [0xe4] #GREEK
0x03c3, //  [0xe5] #GREEK
0x00b5, //  [0xe6] #MICRO
0x03c4, //  [0xe7] #GREEK
0x03a6, //  [0xe8] #GREEK
0x0398, //  [0xe9] #GREEK
0x03a9, //  [0xea] #GREEK
0x03b4, //  [0xeb] #GREEK
0x221e, //  [0xec] #INFINITY
0x03c6, //  [0xed] #GREEK
0x03b5, //  [0xee] #GREEK
0x2229, //  [0xef] #INTERSECTION
0x2261, //  [0xf0] #IDENTICAL
0x00b1, //  [0xf1] #PLUS-MINUS
0x2265, //  [0xf2] #GREATER-THAN
0x2264, //  [0xf3] #LESS-THAN
0x2320, //  [0xf4] #TOP
0x2321, //  [0xf5] #BOTTOM
0x00f7, //  [0xf6] #DIVISION
0x2248, //  [0xf7] #ALMOST
0x00b0, //  [0xf8] #DEGREE
0x2219, //  [0xf9] #BULLET
0x00b7, //  [0xfa] #MIDDLE
0x221a, //  [0xfb] #SQUARE
0x207f, //  [0xfc] #SUPERSCRIPT
0x00b2, //  [0xfd] #SUPERSCRIPT
0x25a0, //  [0xfe] #BLACK
0x00a0 //  [0xff] #NO-BREAK
};


bool set_wwiv_codepage(wwiv_codepage_t cp) {
  std::string default_locale;
#ifdef _WIN32
  if (cp == wwiv_codepage_t::utf8) {
    default_locale = ".UTF-8";
    ::SetConsoleOutputCP(65001); // CP_UTF8
  } else {
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/setlocale-wsetlocale?view=msvc-140
    // OCP is OEM code page, which is OEM 437, IBM 437, etc.
    default_locale = ".OCP";
    ::SetConsoleOutputCP(437); // CP_437
  }

#endif
  // Note, we use LC_CTYPE not LC_ALL so we only affect characters to be UTF-8, but we leave
  // the number formatting, etc as-is (in 'C')
  const auto* new_locale = std::setlocale(LC_CTYPE, default_locale.c_str());
  
  if (new_locale) {
    VLOG(1) << "Resetting Locale: " << new_locale;
  }
  return true;
}

int cp437_to_utf8(uint8_t ch, char* out) {
  const auto unicode = dos_to_utf8_[ch];
  const auto res = wctomb(out, unicode);
  if (res == -1) {
    out[0] = '\0';
    return 0;
  }
  out[res] = '\0';
  return res;
}

wchar_t cp437_to_utf8(char ch) {
  return cp437_to_utf8(static_cast<uint8_t>(ch));
}

wchar_t cp437_to_utf8(uint8_t ch) {
  return dos_to_utf8_[ch];
}

std::wstring cp437_to_utf8w(const std::string& in) {
  std::wstring s;
  s.reserve(in.size());
  for (auto ch : in) {
    s.push_back(dos_to_utf8_[static_cast<uint8_t>(ch)]);
  }
  s.shrink_to_fit();
  return s;
}

std::string cp437_to_utf8(const std::string& in) {
  const auto s = cp437_to_utf8w(in);
  std::string contents;
  const auto max_len = size_int(s) * 3 + 1;
  contents.resize(1024 * 3 + 1);
  const auto new_size = wcstombs(&contents[0], s.c_str(), max_len);
  contents.resize(new_size);
  return contents;
}

}

