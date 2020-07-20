/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
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
#ifndef __INCLUDED_SDK_FILES_ARC_H__
#define __INCLUDED_SDK_FILES_ARC_H__

#include <filesystem>
#include <optional>
#include <string>
#include "sdk/vardec.h"

namespace wwiv::sdk::files {

// https://www.hanshq.net/zip.html

enum class archive_method_t { 
  UNKNOWN = -1,
  ZIP_STORED = 0,
  ZIP_DEFLATED = 1 // 8 
};

struct archive_entry_t {
  std::string filename;
  time_t dt;
  archive_method_t method;
  int32_t compress_size;
  int32_t uncompress_size;
  uint32_t crc32;
};

/**
 * Reads data/archiver.dat and populates a vector of arcrec from it.
 */
std::vector<arcrec> read_arcs(const std::string& datadir);

/**
 * Returns an optional vector of archive_entry_t containing the files of archive
 * file identified by path.
 */
std::optional<std::vector<archive_entry_t>> list_archive(const std::filesystem::path& path);

/**
 * Returns the arc extension for the file identified by filename or std::nullopt of none
 * could be determined.  The contents of the file are checked first and if there is no
 * match then the filename's extension is consulted.
 *
 * This method is ideal when you do not know the archive type, like with QWK packets and
 * also FTN network bundles.
 */
std::optional<std::string> determine_arc_extension(const std::filesystem::path& filename);

/**
 * Returns the arcrec for the archive type identified by filename or std::nullopt of none
 * could be determined.  The contents of the file are checked first and if there is no
 * match then the filename's extension is consulted.
 *
 * This method is ideal when you do not know the archive type, like with QWK packets and
 * also FTN network bundles.
 */
std::optional<arcrec> find_arcrec(const std::vector<arcrec> arcs,
                                  const std::filesystem::path& path);

/**
 * Returns the arcrec for the archive type identified by filename or std::nullopt of none
 * could be determined.  The contents of the file are checked first and if there is no
 * match then the filename's extension is consulted.  
 *
 * Finally if there is no match, use default_ext as the file extension to use when locating
 * the archiver record for this file.
 *
 * This method is ideal when you do not know the archive type, like with QWK packets and
 * also FTN network bundles.
 */
std::optional<arcrec> find_arcrec(const std::vector<arcrec> arcs, const std::filesystem::path& path,
                                  const std::string& default_ext);

} // namespace wwiv::sdk::files

#endif