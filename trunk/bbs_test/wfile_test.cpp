#include "gtest/gtest.h"
#include "platform/incl1.h"
#include "platform/wfile.h"

#include "wstringutils.h"

#include <stdio.h>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef GetFullPathName
#else
#include <sys/stat.h>
#endif

using std::cout;
using std::string;
using wwiv::strings::StringPrintf;

// incl1.h defines mkdir
#ifdef mkdir
#undef mkdir
#endif

class FileTest : public testing::Test {
 public:
    string TempDir() {
#ifdef _WIN32
        char local_dir_template[_MAX_PATH];
        char temp_path[_MAX_PATH];
        GetTempPath(_MAX_PATH, temp_path);
        static char *dir_template = "fnXXXXXX";
        sprintf(local_dir_template, "%s%s", temp_path, dir_template);
        char *result = _mktemp(local_dir_template);
        if (!CreateDirectory(result, NULL)) {
            *result = '\0';
        }
#else
        char local_dir_template[MAX_PATH];
	static const char kTemplate[] = "/tmp/fileXXXXXX";
	strcpy(local_dir_template, kTemplate);
	char *result = mkdtemp(local_dir_template);
#endif
        return std::string(result);
    }

  string CreateTempFile(const string& name, const string& contents) {
    string tmp = TempDir();
    string path = StringPrintf("%s%c%s", tmp.c_str(), WWIV_FILE_SEPERATOR_CHAR,
			       name.c_str());
    FILE* file = fopen(path.c_str(), "w");
    fputs(contents.c_str(), file);
    fclose(file);
    return path;
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
#ifdef _WIN32
    ASSERT_EQ(0, _mkdir(dne.GetFullPathName().c_str()));
#else
    ASSERT_EQ(0, mkdir(dne.GetFullPathName().c_str(), 0777));
#endif  // _WIN32
    ASSERT_TRUE(dne.Exists());
}

TEST_F(FileTest, Length_Open) {
  string path = CreateTempFile("Length_Open", "Hello World");
  WFile file(path);
  file.Open(WFile::modeBinary | WFile::modeReadOnly);
  ASSERT_EQ(11, file.GetLength());
}

TEST_F(FileTest, Length_NotOpen) {
  string path = CreateTempFile("Length_NotOpen", "Hello World");
  WFile file(path);
  ASSERT_EQ(11, file.GetLength());
}
