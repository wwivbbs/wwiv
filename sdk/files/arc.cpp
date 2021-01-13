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
#include "sdk/files/arc.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/log.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::sdk::files {

/* Convert DOS date and time to time_t. */
static time_t dos2time_t(uint16_t dos_date, uint16_t dos_time) {
  struct tm tm{};

  tm.tm_sec = (dos_time & 0x1f) * 2;  /* Bits 0--4:  Secs divided by 2. */
  tm.tm_min = (dos_time >> 5) & 0x3f; /* Bits 5--10: Minute. */
  tm.tm_hour = (dos_time >> 11);      /* Bits 11-15: Hour (0--23). */

  tm.tm_mday = (dos_date & 0x1f);          /* Bits 0--4: Day (1--31). */
  tm.tm_mon = ((dos_date >> 5) & 0xf) - 1; /* Bits 5--8: Month (1--12). */
  tm.tm_year = (dos_date >> 9) + 80;       /* Bits 9--15: Year-1980. */

  tm.tm_isdst = -1;

  return mktime(&tm);
}

///////////////////////////////////////////////////////////////////////////////
// ZIP FILE
//
// https://www.hanshq.net/zip.html


// .ZIP structures and defines
static constexpr uint32_t ZIP_LOCAL_SIG = 0x04034b50;
static constexpr uint32_t ZIP_CENT_START_SIG = 0x02014b50;
static constexpr uint32_t ZIP_CENT_END_SIG = 0x06054b50;

#pragma pack(push, 1)
struct zip_local_header {
  uint32_t signature; // 0x04034b50
  uint16_t extract_ver;
  uint16_t flags;
  uint16_t comp_meth;
  uint16_t mod_time;
  uint16_t mod_date;
  uint32_t crc_32;
  uint32_t comp_size;
  uint32_t uncomp_size;
  uint16_t filename_len;
  uint16_t extra_length;
};

struct zip_central_dir {
  uint32_t signature; // 0x02014b50
  uint16_t made_ver;
  uint16_t extract_ver;
  uint16_t flags;
  uint16_t comp_meth;
  uint16_t mod_time;
  uint16_t mod_date;
  uint32_t crc_32;
  uint32_t comp_size;
  uint32_t uncomp_size;
  uint16_t filename_len;
  uint16_t extra_len;
  uint16_t comment_len;
  uint16_t disk_start;
  uint16_t int_attr;
  uint32_t ext_attr;
  uint32_t rel_ofs_header;
};

struct zip_end_dir {
  uint32_t signature; // 0x06054b50
  uint16_t disk_num;
  uint16_t cent_dir_disk_num;
  uint16_t total_entries_this_disk;
  uint16_t total_entries_total;
  uint32_t central_dir_size;
  uint32_t ofs_cent_dir;
  uint16_t comment_len;
};
#pragma pack(pop)

archive_method_t zip_method(int z) {
  if (z == 0) {
    return archive_method_t::ZIP_STORED;
  }
  if (z == 8) {
    return archive_method_t::ZIP_DEFLATED;
  }
  return archive_method_t::UNKNOWN;
}

template <typename Z> archive_entry_t create_archive_entry(const Z& z, const char* fn) { 
  archive_entry_t a;
  a.filename =  StringTrim(fn);
  a.crc32 = z.crc_32;
  a.dt = dos2time_t(z.mod_date, z.mod_time);
  a.compress_size = z.comp_size;
  a.uncompress_size = z.uncomp_size;
  a.method = zip_method(z.comp_meth);
  return a;
}

static std::optional<std::vector<archive_entry_t>>
list_archive_zip(const std::filesystem::path& path) {
  std::vector<archive_entry_t> files;

  File file(path);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    return std::nullopt;
  }
  long l = 0;
  const auto len = file.length();
  bool done = false;
  while (l < len && !done) {
    long sig = 0;
    file.Seek(l, File::Whence::begin);
    file.Read(&sig, 4);
    file.Seek(l, File::Whence::begin);
    switch (sig) {
    case ZIP_LOCAL_SIG: {
      zip_local_header zl{};
      char s[1024];
      file.Read(&zl, sizeof(zl));
      file.Read(s, zl.filename_len);
      s[zl.filename_len] = '\0';
      // Since zip_central_dir and zip_local_header both have the same
      // information, don't add it here.
      // files.emplace_back(create_archive_entry(zl, s));
      // VLOG(1) << "ZIP_LOCAL_SIG: " << s;
      l += static_cast<long>(sizeof(zl)) + zl.comp_size + zl.filename_len + zl.extra_length;
    } break;
    case ZIP_CENT_START_SIG: {
      zip_central_dir zc{};
      char s[1024];
      file.Read(&zc, sizeof(zc));
      file.Read(s, zc.filename_len);
      s[zc.filename_len] = '\0';
      VLOG(1) << "ZIP_CENT_START_SIG: " << s;
      files.emplace_back(create_archive_entry(zc, s));
      l += sizeof(zc);
      l += zc.filename_len + zc.extra_len;
    } break;
    case ZIP_CENT_END_SIG:
      [[fallthrough]];
    default: 
      done = true;
      break;
    }
  }
  file.Close();
  return {files};
}

///////////////////////////////////////////////////////////////////////////////
// ARC FILE
// http://fileformats.archiveteam.org/wiki/ARC_(compression_format)

#pragma pack(push, 1)
struct arch {
  uint8_t type;
  char name[13];
  int32_t len;
  int16_t date, time, crc;
  int32_t size;
};
#pragma pack(pop)

static std::optional<std::vector<archive_entry_t>> list_archive_arc(const std::filesystem::path& path) {
  File file(path);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    return std::nullopt;
  }
  arch a{};
  const auto file_size = file.length();
  long pos = 1;
  file.Seek(0, File::Whence::begin);
  file.Read(&a, 1);
  if (a.type != 0x1a) {
    return std::nullopt;
  }

  std::vector<archive_entry_t> files;
  while (pos < file_size) {
    file.Seek(pos, File::Whence::begin);
    const auto num_read = file.Read(&a, sizeof(arch));
    if (num_read != sizeof(arch)) {
      // early EOF
      return files;
    }
    pos += sizeof(arch);
    if (a.type == 1) {
      pos -= 4;
      a.size = a.len;
    }
    if (!a.type) {
      return files;
    }
    pos += a.len;
    ++pos;
    archive_entry_t ae{};
    const auto arc_fn = trim_to_size(a.name, 13);
    ae.filename = arc_fn;
    ae.crc32 = a.crc;
    ae.compress_size = a.len;
    ae.uncompress_size = a.size;
    ae.dt = dos2time_t(a.date, a.time);
    files.emplace_back(ae);
  }
  return files;
}

///////////////////////////////////////////////////////////////////////////////
// LZH FILE
// http://www33146ue.sakura.ne.jp/staff/iz/formats/lzh.html
// http://web.archive.org/web/20021219165813/http://www.osirusoft.com/joejared/lzhformat.html
//
#pragma pack(push, 1)

// This doesn't include the size bit at the front
// or   uint8_t fn_len at the end of level 0 and 1
 
struct lharc_header {
  uint8_t checksum;
  char ctype[5];
  int32_t comp_size;
  int32_t uncomp_size;
  uint16_t time;
  uint16_t date;
  uint8_t attr;
  uint8_t level;
};
#pragma pack(pop)

static std::optional<std::vector<archive_entry_t>> list_archive_lzh(const std::filesystem::path& path) {

  const auto fn = path.filename().string();
  File file(path);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    return std::nullopt;
  }
  std::vector<archive_entry_t> files;
  const auto file_size = file.length();

  for (long l = 0; l < file_size; ) {
    lharc_header a{};
    file.Seek(l, File::Whence::begin);
    char flag;
    file.Read(&flag, 1);
    if (!flag) {
      break;
    }
    const auto num_read = file.Read(&a, sizeof(lharc_header));
    if (num_read != sizeof(lharc_header)) {
      // Early EOF
      return {files};
    }

    archive_entry_t ae{};
    if (a.level == 0 || a.level == 1) {
      /*
       * 21      1 byte   Filename / path length in bytes (f)
       * 22     (f)bytes  Filename / path
       * 22+(f)  2 bytes  CRC-16 of original file
       * [[ LEVEL 0 ]]
       * 24+(f) (n)bytes  Compressed data
       * [[ LEVEL 1 ]]
       * 24+(f)  1 byte   OS ID
       * 25+(f)  2 bytes  Next header size(x) (0 means no extension header)
       * [ // Extension headers
       *         1 byte   Extension type
       *     (x)-3 bytes  Extension fields
       *         2 bytes  Next header size(x) (0 means no next extension header)
       * ]*
       *        (n)bytes  Compressed data
       */

      uint8_t fn_len;
      if (1 != file.Read(&fn_len, 1)) {
        LOG(ERROR) << "Error reading fn_len" << " on file: " << path;
        return {files};
      }

      char buffer[256];
      if (fn_len != file.Read(buffer, fn_len)) {
        // Early EOF
        return {files};
      }
      buffer[fn_len] = '\0';
      ae.filename = buffer;

      uint16_t crc;
      file.Read(&crc, sizeof(crc));
      l += static_cast<int>(sizeof(lharc_header) + fn_len + sizeof(fn_len) + sizeof(crc) + sizeof(flag)) + a.comp_size;
      if (a.level == 1) {
        // Read extra headers and OS ID
        uint8_t os_id;
        if (1 != file.Read(&os_id, 1)) {
          LOG(ERROR) << "Error reading os_id" << " on file: " << path;
          return {files};
        }
        uint16_t ext_size = 0;
        if (2 != file.Read(&ext_size, 2)) {
          LOG(ERROR) << "Error reading os_id"<< " on file: " << path;
          return {files};
        }
        l += sizeof(uint8_t) + sizeof(uint16_t); // os_id and ext_size

        do {
          if (ext_size < 3) {
            LOG(ERROR) << "Invalid ext_size on" << " on file: " << path;
            return {files};
          }
          char ext[1024];
          if (ext_size > 1024) {
            LOG(ERROR) << "Huge ext_size on file: " << path;
            return {files};
          }
          if (ext_size != file.Read(&ext, ext_size)) {
            LOG(ERROR) << "Invalid ext on" << " on file: " << path;
            return {files};
          }

          uint8_t ext_type = ext[0];
          VLOG(1) << "ext_type: " << ext_type;
          ext_size = ext[ext_size - 1] << 8 | ext[ext_size - 2];
        } while (ext_size != 0);
      }
    } else {
      LOG(ERROR) << "Unknown LZH level: " << a.level << " on file: " << path;
      return {files};
    }
    ae.dt = dos2time_t(a.date, a.time);
    ae.compress_size = a.comp_size;
    ae.uncompress_size = a.uncomp_size;
    ae.crc32 = a.checksum;
    files.emplace_back(ae);
  }
  return {files};
}


///////////////////////////////////////////////////////////////////////////////
// ARJ FILE
// http://manual.freeshell.org/unarj/technote.txt
//

#pragma pack(push, 1)

struct arj_header_t {
  uint16_t header_id; // 0x60ea
  // basic header size (from 'first_hdr_size' thru 'comment' below)
  // = first_hdr_size + strlen(filename) + 1 + strlen(comment) + 1 =
  //      0 if end of archive maximum header size is 2600
  uint16_t basic_header_size;
  // (size up to and including 'extra data')
  uint8_t first_hdr_size;
  // archiver version number
  uint8_t archiver_version;
  // minimum archiver version to extract
  uint8_t min_archiver_version;
  //(0 = MSDOS, 1 = PRIMOS, 2 = UNIX, 3 = AMIGA, 4 = MAC - OS)
  // (5 = OS / 2, 6 = APPLE GS, 7 = ATARI ST,
  // 8 = NEXT)(9 = VAX VMS)
  uint8_t host_os;
  // (0x01 = GARBLED_FLAG) indicates password protected file
  // (0x02 = NOT USED)
  // (0x04 = VOLUME_FLAG)  indicates continued file to next volume (file is split)
  // (0x08 = EXTFILE_FLAG) indicates file starting position field (for split files)
  // (0x10 = PATHSYM_FLAG) indicates filename translated ("\" changed to "/")
  // (0x20 = BACKUP_FLAG)  indicates file marked as backup
  uint8_t flags;

  //(0 = stored, 1 = compressed most ... 4 compressed fastest)
  uint8_t method;

  // (0 = binary,    1 = 7-bit text) (3 = directory, 4 = volume label)
  uint8_t file_type;
  uint8_t reserved;
  // date time modified
  uint16_t time;
  uint16_t date;
  uint32_t compressed_size;
  uint32_t original_size;
  // original file's CRC
  uint32_t crc32;
  // filespec position in filename 
  uint16_t filespec_pos;
  // file access mode
  uint16_t fileaccess_mode;
  // host data (currently not used)
  uint16_t host_data;
};

#pragma pack(pop)

static std::optional<std::vector<archive_entry_t>> list_archive_arj(const std::filesystem::path& path) {

  const auto fn = path.filename().string();
  File file(path);
  if (!file.Open(File::modeBinary | File::modeReadOnly)) {
    return std::nullopt;
  }
  std::vector<archive_entry_t> files;
  const auto file_size = file.length();
  long pos = 0;
  file.Seek(0L, File::Whence::begin);
  bool file_header = true;
  while (pos < file_size) {
    arj_header_t h{};
    file.Seek(pos, File::Whence::begin);
    uint16_t magic;
    if (2 != file.Read(&magic, 2) || magic != 0xea60) {
      // LOG(error) << "EOF: " << std::hex << magic;
      return {files};
    }
    file.Seek(pos, File::Whence::begin);
    file.Read(&h, sizeof(h));
    if (h.basic_header_size == 0) {
      // End of Archive
      return {files};
    }
    int header_sizeof = sizeof(arj_header_t) - 4;
    int ext_data_size = h.first_hdr_size - header_sizeof;
    char ext_data[200];
    if (ext_data_size > 0) {
      // Have extra header;
      file.Read(ext_data, ext_data_size);
    }
    auto file_and_comment_size = h.basic_header_size - h.first_hdr_size;
    auto buffer = std::make_unique<char[]>(file_and_comment_size);
    file.Read(buffer.get(), file_and_comment_size);
    std::string filename = buffer.get();
    // 4 for first two fields, 4 for header CRC, and 2 for ext header.
    // We can skip the ext header since arj never used it.
    pos += h.basic_header_size + 4 + 4 + 2;
    if (!file_header) {
      pos += static_cast<long>(h.compressed_size);
      archive_entry_t ae{};
      ae.filename = filename;
      ae.compress_size = h.compressed_size;
      ae.uncompress_size = h.original_size;
      ae.crc32 = h.crc32;
      ae.dt = dos2time_t(h.date, h.time);
      files.emplace_back(ae);
    }
    file_header = false;
    if (h.flags & 0x08) {
      pos += 4;
    }
  }
  return {files};
}


///////////////////////////////////////////////////////////////////////////////
// Generic Archive
//

std::optional<std::vector<archive_entry_t>> list_archive(const std::filesystem::path& path) {
  struct arc_command {
    const std::string arc_name;
    std::function<std::optional<std::vector<archive_entry_t>>(const std::filesystem::path&)> func;
  };

  static const std::vector<arc_command> arc_t = {
    {"ZIP", list_archive_zip}, 
    {"ARC", list_archive_arc},
    {"LZH", list_archive_lzh},
    {"ARJ", list_archive_arj},
  };

  if (!path.has_extension()) {
    // no extension?
    return std::nullopt;
  }
  auto ext = ToStringUpperCase(path.extension().string());
  if (ext.front() == '.') {
    // trim leading . for extension
    ext = ext.substr(1);
  }
  for (const auto& t : arc_t) {
    if (iequals(ext, t.arc_name)) {
      return t.func(path);
    }
  }
  return std::nullopt;
}

// One thing to note, if an 'arc' is found, it uses pak, and returns that
// The reason being, PAK does all ARC does, plus a little more, I believe
// PAK has its own special modes, but still look like an ARC, thus, an ARC
// Will puke if it sees this
std::optional<std::string> determine_arc_extension(const std::filesystem::path& filename) {
  File f(filename);
  if (!f.Open(File::modeReadOnly | File::modeBinary)) {
    return std::nullopt;
  }

  char header[10];
  const auto num_read = f.Read(&header, 10);
  if (num_read < 10) {
    return std::nullopt;
  }

  switch (header[0]) {
  case 0x60:
    if (static_cast<unsigned char>(header[1]) == static_cast<unsigned char>(0xEA))
      return {"ARJ"};
    break;
  case 0x1a:
    return {"ARC"};
  case 'P':
    if (header[1] == 'K')
      return {"ZIP"};
    break;
  case 'R':
    if (header[1] == 'a')
      return {"RAR"};
    break;
  case 'Z':
    if (header[1] == 'O' && header[2] == 'O')
      return {"ZOO"};
    break;
  }
  if (header[0] == 'P') {
    return std::nullopt;
  }
  header[9] = 0;
  if (strstr(header, "-lh")) {
    return {"LZH"};
  }
  return std::nullopt;
}

std::vector<arcrec> read_arcs(const std::string& datadir) {
  std::vector<arcrec> arcs;
  if (auto file = DataFile<arcrec>(FilePath(datadir, ARCHIVER_DAT))) {
    file.ReadVector(arcs, 20);
  }
  return arcs;
}

/** returns the arcrec for the extension, or the 1st one if none match */
static std::optional<arcrec> find_arc_by_extension(const std::vector<arcrec> arcs, const std::string& extension) {
  const auto ue = ToStringUpperCase(extension);
  for (const auto& a : arcs) {
    if (ue == a.extension) {
      return {a};
    }
  }
  return std::nullopt;
}

std::optional<arcrec> find_arcrec(const std::vector<arcrec>& arcs,
                                  const std::filesystem::path& path) {
  const auto ext = determine_arc_extension(path);
  if (!ext) {
    return std::nullopt;
  }
  return find_arc_by_extension(arcs, ext.value());
}

std::optional<arcrec> find_arcrec(const std::vector<arcrec> arcs, const std::filesystem::path& path,
                                  const std::string& default_ext) {
  const auto ext = determine_arc_extension(path);
  if (!ext) {
    return find_arc_by_extension(arcs, ToStringUpperCase(default_ext));
  }
  return find_arc_by_extension(arcs, ext.value());
}

} // namespace wwiv::sdk::files