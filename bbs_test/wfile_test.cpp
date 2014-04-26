#include "gtest/gtest.h"
#include <string>
//#include <io.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "platform/WFile.h"

static char *dir_template = "fnXXXXXX";

using std::cout;
using std::string;

class FileTest : public testing::Test {
 public:
    string TempDir() {
        char local_dir_template[_MAX_PATH];
        char temp_path[_MAX_PATH];
        GetTempPath(_MAX_PATH, temp_path);
        sprintf(local_dir_template, "%s%s", temp_path, dir_template);
        char *result = _mktemp(local_dir_template);
        if (!CreateDirectory(result, NULL)) {
            *result = '\0';
        }
        return std::string(result);
    }
};

TEST_F(FileTest, DoesNotExist) {
    string tmp = TempDir();
    GTEST_ASSERT_NE("", tmp);
    WFile dne(tmp, "doesnotexist");
    ASSERT_FALSE(dne.Exists());
}

TEST_F(FileTest, Exists) {
    string tmp = TempDir();
    GTEST_ASSERT_NE("", tmp);
    WFile dne(tmp, "newdir");
    ASSERT_EQ(0, _mkdir(dne.GetFullPathName().c_str()));
    ASSERT_TRUE(dne.Exists());
}
