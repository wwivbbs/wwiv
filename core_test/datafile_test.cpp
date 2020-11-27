/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2020, WWIV Software Services           */
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
#include "core/datafile.h"
#include "core/file.h"
#include "core/strings.h"
#include <string>

using std::string;
using namespace wwiv::core;
using namespace wwiv::strings;

TEST(DataFileTest, Read) {
  struct T { int a; int b; };
  FileHelper file;
  const auto& tmp = file.TempDir();

  File x(FilePath(tmp, "Read"));
  ASSERT_TRUE(x.Open(File::modeCreateFile|File::modeBinary|File::modeReadWrite));
  T t1{1, 2};
  T t2{3, 4};
  x.Write(&t1, sizeof(T));
  x.Write(&t2, sizeof(T));
  x.Close();

  {
    DataFile<T> datafile(FilePath(tmp, "Read"), File::modeReadOnly);
    ASSERT_TRUE(static_cast<bool>(datafile));
    EXPECT_EQ(2, datafile.number_of_records());
    T t{0, 0};
    EXPECT_TRUE(datafile.Read(&t));
    EXPECT_EQ(1, t.a);
    EXPECT_EQ(2, t.b);

    EXPECT_TRUE(datafile.Read(&t));
    EXPECT_EQ(3, t.a);
    EXPECT_EQ(4, t.b);

    EXPECT_TRUE(datafile.Read(1, &t));
    EXPECT_EQ(3, t.a);
    EXPECT_EQ(4, t.b);

    EXPECT_TRUE(datafile.Read(0, &t));
    EXPECT_EQ(1, t.a);
    EXPECT_EQ(2, t.b);
  }
}

TEST(DataFileTest, ReadVector) {
  struct T { int a; int b; };
  FileHelper file;
  const auto& tmp = file.TempDir();

  File x(FilePath(tmp, "ReadVector"));
  ASSERT_TRUE(x.Open(File::modeCreateFile | File::modeBinary | File::modeReadWrite));
  T t1{1, 2};
  T t2{3, 4};
  T t3{5, 6};
  x.Write(&t1, sizeof(T));
  x.Write(&t2, sizeof(T));
  x.Write(&t3, sizeof(T));
  x.Close();

  {
    DataFile<T> datafile(FilePath(tmp, "ReadVector"), File::modeReadOnly);
    ASSERT_TRUE(static_cast<bool>(datafile));
    EXPECT_EQ(3, datafile.number_of_records());
    std::vector<T> t;
    EXPECT_TRUE(datafile.ReadVector(t));
    EXPECT_EQ(static_cast<size_t>(3), t.size());
    EXPECT_EQ(1, t[0].a);
    EXPECT_EQ(2, t[0].b);
    EXPECT_EQ(3, t[1].a);
    EXPECT_EQ(4, t[1].b);
    EXPECT_EQ(5, t[2].a);
    EXPECT_EQ(6, t[2].b);
  }
}

TEST(DataFileTest, ReadVector_Empty) {
  struct T { int a; int b; };
  const FileHelper file{};
  const auto& tmp = file.TempDir();

  {
    File x(FilePath(tmp, "ReadVector_Empty"));
    ASSERT_TRUE(x.Open(File::modeCreateFile | File::modeBinary | File::modeReadWrite));
    x.Close();
  }

  DataFile<T> datafile(FilePath(tmp, "ReadVector_Empty"), File::modeReadOnly);
  ASSERT_TRUE(static_cast<bool>(datafile));
  EXPECT_EQ(0, datafile.number_of_records());
  std::vector<T> t{};
  EXPECT_TRUE(datafile.ReadVector(t));
  EXPECT_EQ(static_cast<size_t>(0), t.size());
}

TEST(DataFileTest, ReadVector_MaxRecords) {
  struct T { int a; int b; };
  FileHelper file;
  const auto& tmp = file.TempDir();

  File x(FilePath(tmp, "ReadVector_MaxRecords"));
  ASSERT_TRUE(x.Open(File::modeCreateFile | File::modeBinary | File::modeReadWrite));
  T t1{1, 2};
  T t2{3, 4};
  T t3{5, 6};
  x.Write(&t1, sizeof(T));
  x.Write(&t2, sizeof(T));
  x.Write(&t3, sizeof(T));
  x.Close();

  {
    DataFile<T> datafile(FilePath(tmp, "ReadVector_MaxRecords"), File::modeReadOnly);
    ASSERT_TRUE(static_cast<bool>(datafile));
    EXPECT_EQ(3, datafile.number_of_records());
    std::vector<T> t;
    EXPECT_TRUE(datafile.ReadVector(t, 2));
    EXPECT_EQ(static_cast<size_t>(2), t.size());
    EXPECT_EQ(1, t[0].a);
    EXPECT_EQ(2, t[0].b);
    EXPECT_EQ(3, t[1].a);
    EXPECT_EQ(4, t[1].b);
  }
}

TEST(DataFileTest, Write) {
  struct T { int a; int b; };
  FileHelper file;
  const auto& tmp = file.TempDir();

  T t1{1, 2};
  T t2{3, 4};

  {
    DataFile<T> datafile(FilePath(tmp, "Write"),
                         File::modeCreateFile | File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(static_cast<bool>(datafile));
    datafile.Write(&t1);
    datafile.Write(&t2);
  }
  File x(FilePath(tmp, "Write"));
  ASSERT_TRUE(x.Open(File::modeBinary|File::modeReadOnly));
  x.Read(&t1, sizeof(T));
  x.Read(&t2, sizeof(T));
  x.Close();
  EXPECT_EQ(1, t1.a);
  EXPECT_EQ(2, t1.b);
  EXPECT_EQ(3, t2.a);
  EXPECT_EQ(4, t2.b);
}

TEST(DataFileTest, WriteVector) {
  struct T { int a; int b; };
  FileHelper file;
  const auto& tmp = file.TempDir();

  T t1{1, 2};
  T t2{3, 4};

  {
    DataFile<T> datafile(FilePath(tmp, "WriteVector"),
        File::modeCreateFile | File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(static_cast<bool>(datafile));
    std::vector<T> t = {t1, t2};
    datafile.WriteVector(t);
  }
  File x(FilePath(tmp, "WriteVector"));
  ASSERT_TRUE(x.Open(File::modeBinary | File::modeReadOnly));
  x.Read(&t1, sizeof(T));
  EXPECT_EQ(1, t1.a);
  EXPECT_EQ(2, t1.b);
  x.Read(&t2, sizeof(T));
  EXPECT_EQ(3, t2.a);
  EXPECT_EQ(4, t2.b);
  x.Close();
}

TEST(DataFileTest, WriteVector_Empty) {
  struct T { int a; int b; };
  const FileHelper file{};
  const auto& tmp = file.TempDir();

  {
    DataFile<T> datafile(FilePath(tmp, "WriteVector_Empty"),
        File::modeCreateFile | File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(static_cast<bool>(datafile));
    const std::vector<T> t{};
    EXPECT_TRUE(datafile.WriteVector(t));
  }
  File x(FilePath(tmp, "WriteVector_Empty"));
  ASSERT_TRUE(x.Open(File::modeBinary | File::modeReadOnly));
  EXPECT_EQ(0, x.length());
}

TEST(DataFileTest, WriteVector_MaxRecords) {
  struct T { int a; int b; };
  FileHelper file;
  const auto& tmp = file.TempDir();

  T t1{1, 2};
  T t2{3, 4};
  T t3{5, 6};

  {
    DataFile<T> datafile(FilePath(tmp, "WriteVector_MaxRecords"),
      File::modeCreateFile | File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(static_cast<bool>(datafile));
    std::vector<T> t = {t1, t2, t3};
    datafile.WriteVector(t, 2);
  }
  File x(FilePath(tmp, "WriteVector_MaxRecords"));
  ASSERT_TRUE(x.Open(File::modeBinary | File::modeReadOnly));
  ASSERT_EQ(static_cast<long>(2 * sizeof(T)), x.length());
  x.Read(&t1, sizeof(T));
  EXPECT_EQ(1, t1.a);
  EXPECT_EQ(2, t1.b);
  x.Read(&t2, sizeof(T));
  EXPECT_EQ(3, t2.a);
  EXPECT_EQ(4, t2.b);
  x.Close();
}

TEST(DataFileTest, WriteVectorAndTruncate) {
  struct T { int a; int b; };
  FileHelper file;
  const auto& tmp = file.TempDir();

  T t1{1, 2};
  T t2{3, 4};
  const auto path = FilePath(tmp, "WriteVectorAndTruncate");
  File f(path); 

  {
    // Create file containing two files.
    DataFile<T> datafile(path, File::modeCreateFile | File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(static_cast<bool>(datafile));
    std::vector<T> t = {t1, t2};
    datafile.WriteVector(t);
    ASSERT_EQ(f.length(), static_cast<File::size_type>(2 * sizeof(T)));
  }

  {
    DataFile<T> datafile(path, File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(static_cast<bool>(datafile));
    datafile.WriteVector({t1});
    // Should still 
    ASSERT_EQ(f.length(), static_cast<File::size_type>(2 * sizeof(T)));
  }

  {
    DataFile<T> datafile(path, File::modeBinary | File::modeReadWrite);
    ASSERT_TRUE(static_cast<bool>(datafile));
    datafile.WriteVectorAndTruncate({t1});
    ASSERT_EQ(f.length(), static_cast<File::size_type>(sizeof(T)));
  }

}

TEST(DataFileTest, Read_DoesNotExist) {
  struct T { int a; };
  const FileHelper file;
  const auto& tmp = file.TempDir();
  const DataFile<T> datafile(FilePath(tmp, "DoesNotExist"), File::modeBinary | File::modeReadWrite);
  if (datafile) {
    FAIL() << "file should not exist.";
  }
  EXPECT_FALSE(datafile);
}
