/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2014, WWIV Software Services                  */
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

#include <io.h>
#include <stdio.h>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef GetFullPathName
#else
#include <sys/stat.h>
#endif

#include "platform/incl1.h"
#include "wstringutils.h"

// incl1.h defines mkdir
#ifdef mkdir
#undef mkdir
#endif

std::string FileHelper::TempDir() {
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

std::string FileHelper::CreateTempFile(const std::string& name, const std::string& contents) {
    std::string tmp = TempDir();
    std::string path = wwiv::strings::StringPrintf("%s%c%s", tmp.c_str(), 
        WWIV_FILE_SEPERATOR_CHAR, name.c_str());
    FILE* file = fopen(path.c_str(), "w");
    fputs(contents.c_str(), file);
    fclose(file);
    return path;
}