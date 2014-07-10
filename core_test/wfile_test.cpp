/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#include "file_helper.h"
#include "gtest/gtest.h"
#include "core/wfile.h"

#include <iostream>
#include <string>

using std::string;

TEST(FileTest, DoesNotExist) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    WFile dne(tmp, "doesnotexist");
    ASSERT_FALSE(dne.Exists());
}

TEST(FileTest, DoesNotExist_Static) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    WFile dne(tmp, "doesnotexist");
    ASSERT_FALSE(WFile::Exists(dne.GetFullPathName()));
}

TEST(FileTest, Exists) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    WFile dne(tmp, "newdir");
    ASSERT_TRUE(dne.Exists()) << dne.GetFullPathName(); 
}

TEST(FileTest, Exists_Static) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    WFile dne(tmp, "newdir");
    ASSERT_TRUE(WFile::Exists(dne.GetFullPathName())) << dne.GetFullPathName(); 
}

TEST(FileTest, Length_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile("Length_Open", kHelloWorld);
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    ASSERT_EQ(kHelloWorld.size(), file.GetLength());
}

TEST(FileTest, Length_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile("Length_NotOpen", kHelloWorld);
    WFile file(path);
    ASSERT_EQ(kHelloWorld.size(), file.GetLength());
}

TEST(FileTest, IsDirectory_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile("Length_Open", kHelloWorld);
    WFile file(path);
    ASSERT_FALSE(file.IsDirectory());
    ASSERT_TRUE(file.IsFile());
}

TEST(FileTest, IsDirectory_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    string path = helper.CreateTempFile("Length_Open", kHelloWorld);
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    ASSERT_FALSE(file.IsDirectory());
    ASSERT_TRUE(file.IsFile());
}

TEST(FileTest, GetFileTime_NotOpen) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    time_t now = time(nullptr);
    string path = helper.CreateTempFile("Length_Open", kHelloWorld);
    WFile file(path);
    ASSERT_LE(now, file.GetFileTime());
}

TEST(FileTest, GetFileTime_Open) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    time_t now = time(nullptr);
    string path = helper.CreateTempFile("Length_Open", kHelloWorld);
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    ASSERT_LE(now, file.GetFileTime());
}

TEST(FileTest, Read) {
    static const string kHelloWorld = "Hello World";
    FileHelper helper;
    time_t now = time(nullptr);
    string path = helper.CreateTempFile("Length_Open", kHelloWorld);
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    char buf[255];
    ASSERT_EQ(kHelloWorld.length(), file.Read(buf, kHelloWorld.length()));
    buf[11] = 0;
    ASSERT_STREQ(kHelloWorld.c_str(), buf);
}
