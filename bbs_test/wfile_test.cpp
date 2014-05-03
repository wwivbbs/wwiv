#include "file_helper.h"
#include "gtest/gtest.h"
#include "platform/wfile.h"

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
    WFile dne(tmp, "newdir");
#ifdef _WIN32
    ASSERT_EQ(0, _mkdir(dne.GetFullPathName().c_str()));
#else
    ASSERT_EQ(0, mkdir(dne.GetFullPathName().c_str(), 0777));
#endif  // _WIN32
    ASSERT_TRUE(dne.Exists());
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
