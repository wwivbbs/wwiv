/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*           Copyright (C)2008-2014, WWIV Software Services                */
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
#include "gtest/gtest.h"

#include "core/strings.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;

TEST(StringsTest, StringColors) {
    EXPECT_EQ( string(""), stripcolors(string("")) );
    EXPECT_EQ( string("|"), stripcolors(string("|")) );
    EXPECT_EQ( string("|0"), stripcolors(string("|0")) );
    EXPECT_EQ( string("12345"), stripcolors(string("12345")) );
    EXPECT_EQ( string("abc"), stripcolors(string("abc")) );
    EXPECT_EQ( string("1 abc"), stripcolors(string("\x031 abc")) );
    EXPECT_EQ( string("\x03 abc"), stripcolors(string("\x03 abc")) );
    EXPECT_EQ( string("abc"), stripcolors(string("|15abc")) );
}

TEST(StringsTest, Properize) {
    EXPECT_EQ( string("Rushfan"), properize( string("rushfan") ) );
    EXPECT_EQ( string("Rushfan"), properize( string("rUSHFAN") ) );
    EXPECT_EQ( string(""), properize( string("") ) );
    EXPECT_EQ( string(" "), properize( string(" ") ) );
    EXPECT_EQ( string("-"), properize( string("-") ) );
    EXPECT_EQ( string("."), properize( string(".") ) );
    EXPECT_EQ( string("R"), properize( string("R") ) );
    EXPECT_EQ( string("R"), properize( string("r") ) );
    EXPECT_EQ( string("Ru"), properize( string("RU") ) );
    EXPECT_EQ( string("R.U"), properize( string("r.u") ) );
    EXPECT_EQ( string("R U"), properize( string("r u") ) );
    EXPECT_EQ( string("Rushfan"), properize( string("Rushfan") ) );
}
