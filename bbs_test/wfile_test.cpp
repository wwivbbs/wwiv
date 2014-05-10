#include "file_helper.h"
#include "gtest/gtest.h"
#include "platform/wfile.h"

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

TEST(FileTest, Exists) {
    FileHelper file;
    string tmp = file.TempDir();
    GTEST_ASSERT_NE("", tmp);
    ASSERT_TRUE(file.Mkdir("newdir"));
    WFile dne(tmp, "newdir");
    ASSERT_TRUE(dne.Exists()) << dne.GetFullPathName(); 
}

TEST(FileTest, Length_Open) {
    FileHelper helper;
    string path = helper.CreateTempFile("Length_Open", "Hello World");
    WFile file(path);
    file.Open(WFile::modeBinary | WFile::modeReadOnly);
    ASSERT_EQ(11, file.GetLength());
}

TEST(FileTest, Length_NotOpen) {
    FileHelper helper;
    string path = helper.CreateTempFile("Length_NotOpen", "Hello World");
    WFile file(path);
    ASSERT_EQ(11, file.GetLength());
}
