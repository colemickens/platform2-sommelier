// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/http/http_form_data.h>

#include <set>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <chromeos/mime_utils.h>
#include <gtest/gtest.h>

namespace chromeos {
namespace http {

TEST(HttpFormData, TextFormField) {
  TextFormField form_field{"field1", "abcdefg", mime::text::kPlain, "7bit"};
  const char expected_header[] =
      "Content-Disposition: form-data; name=\"field1\"\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Transfer-Encoding: 7bit\r\n"
      "\r\n";
  EXPECT_EQ(expected_header, form_field.GetContentHeader());
  EXPECT_EQ(7u, form_field.GetDataSize());
  char buffer[100];
  size_t read = 0;
  EXPECT_TRUE(form_field.ReadData(buffer, sizeof(buffer), &read, nullptr));
  EXPECT_EQ(7u, read);
  EXPECT_EQ("abcdefg", (std::string{buffer, read}));
}

TEST(HttpFormData, FileFormField) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  std::string file_content{"text line1\ntext line2\n"};
  base::FilePath file_name = dir.path().Append("sample.txt");
  ASSERT_EQ(file_content.size(),
            static_cast<size_t>(base::WriteFile(file_name, file_content.data(),
                                                file_content.size())));

  base::File file{file_name, base::File::FLAG_READ | base::File::FLAG_OPEN};
  ASSERT_TRUE(file.IsValid());
  FileFormField form_field{"test_file", file.Pass(), "sample.txt",
                           content_disposition::kFormData,
                           mime::text::kPlain, ""};
  const char expected_header[] =
      "Content-Disposition: form-data; name=\"test_file\";"
      " filename=\"sample.txt\"\r\n"
      "Content-Type: text/plain\r\n"
      "\r\n";
  EXPECT_EQ(expected_header, form_field.GetContentHeader());
  EXPECT_EQ(file_content.size(), form_field.GetDataSize());
  char buffer[100];
  size_t read = 0;
  EXPECT_TRUE(form_field.ReadData(buffer, sizeof(buffer), &read, nullptr));
  EXPECT_EQ(file_content.size(), read);
  EXPECT_EQ(file_content, (std::string{buffer, read}));
}

TEST(HttpFormData, MultiPartFormField) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  std::string file1{"text line1\ntext line2\n"};
  base::FilePath filename1 = dir.path().Append("sample.txt");
  ASSERT_EQ(file1.size(),
            static_cast<size_t>(base::WriteFile(filename1, file1.data(),
                                                file1.size())));
  std::string file2{"\x01\x02\x03\x04\x05"};
  base::FilePath filename2 = dir.path().Append("test.bin");
  ASSERT_EQ(file2.size(),
            static_cast<size_t>(base::WriteFile(filename2, file2.data(),
                                                file2.size())));

  MultiPartFormField form_field{"foo", mime::multipart::kFormData, "Delimiter"};
  form_field.AddTextField("name", "John Doe");
  EXPECT_TRUE(form_field.AddFileField("file1", filename1,
                                      content_disposition::kFormData,
                                      mime::text::kPlain, nullptr));
  EXPECT_TRUE(form_field.AddFileField("file2", filename2,
                                      content_disposition::kFormData,
                                      mime::application::kOctet_stream,
                                      nullptr));
  const char expected_header[] =
      "Content-Disposition: form-data; name=\"foo\"\r\n"
      "Content-Type: multipart/form-data; boundary=\"Delimiter\"\r\n"
      "\r\n";
  EXPECT_EQ(expected_header, form_field.GetContentHeader());
  std::string all_data;
  for (;;) {
    char buffer[1000];
    size_t read = 0;
    ASSERT_TRUE(form_field.ReadData(buffer, sizeof(buffer), &read, nullptr));
    if (read == 0)
      break;
    all_data.append(buffer, buffer + read);
  }
  const char expected_data[] =
      "--Delimiter\r\n"
      "Content-Disposition: form-data; name=\"name\"\r\n"
      "\r\n"
      "John Doe\r\n"
      "--Delimiter\r\n"
      "Content-Disposition: form-data; name=\"file1\";"
      " filename=\"sample.txt\"\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "\r\n"
      "text line1\ntext line2\n\r\n"
      "--Delimiter\r\n"
      "Content-Disposition: form-data; name=\"file2\";"
      " filename=\"test.bin\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "\r\n"
      "\x01\x02\x03\x04\x05\r\n"
      "--Delimiter--";
  EXPECT_EQ(expected_data, all_data);
  EXPECT_EQ(all_data.size(), form_field.GetDataSize());
}

TEST(HttpFormData, MultiPartBoundary) {
  const int count = 10;
  std::set<std::string> boundaries;
  for (int i = 0; i < count; i++) {
    MultiPartFormField field{""};
    std::string boundary = field.GetBoundary();
    boundaries.insert(boundary);
    // Our generated boundary must be 16 character long and contain lowercase
    // hexadecimal digits only.
    EXPECT_EQ(16u, boundary.size());
    EXPECT_EQ(std::string::npos,
              boundary.find_first_not_of("0123456789abcdef"));
  }
  // Now make sure the boundary strings were generated at random, so we should
  // get |count| unique boundary strings. However since the strings are random,
  // there is a very slim change of generating the same string twice, so
  // expect at least 90% of unique strings. 90% is picked arbitrarily here.
  int expected_min_unique = count * 9 / 10;
  EXPECT_GE(boundaries.size(), expected_min_unique);
}

TEST(HttpFormData, FormData) {
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  std::string file1{"text line1\ntext line2\n"};
  base::FilePath filename1 = dir.path().Append("sample.txt");
  ASSERT_EQ(file1.size(),
            static_cast<size_t>(base::WriteFile(filename1, file1.data(),
                                                file1.size())));
  std::string file2{"\x01\x02\x03\x04\x05"};
  base::FilePath filename2 = dir.path().Append("test.bin");
  ASSERT_EQ(file2.size(),
            static_cast<size_t>(base::WriteFile(filename2, file2.data(),
                                                file2.size())));

  FormData form_data{"boundary1"};
  form_data.AddTextField("name", "John Doe");
  std::unique_ptr<MultiPartFormField> files{
      new MultiPartFormField{"files", "", "boundary2"}};
  EXPECT_TRUE(files->AddFileField("", filename1,
                                  content_disposition::kFile,
                                  mime::text::kPlain, nullptr));
  EXPECT_TRUE(files->AddFileField("", filename2,
                                  content_disposition::kFile,
                                  mime::application::kOctet_stream,
                                  nullptr));
  form_data.AddCustomField(std::move(files));
  EXPECT_EQ("multipart/form-data; boundary=\"boundary1\"",
            form_data.GetContentType());
  std::string all_data;
  for (;;) {
    char buffer[8];
    size_t read = 0;
    ASSERT_TRUE(form_data.ReadData(buffer, sizeof(buffer), &read, nullptr));
    if (read == 0)
      break;
    all_data.append(buffer, buffer + read);
  }
  const char expected_data[] =
      "--boundary1\r\n"
      "Content-Disposition: form-data; name=\"name\"\r\n"
      "\r\n"
      "John Doe\r\n"
      "--boundary1\r\n"
      "Content-Disposition: form-data; name=\"files\"\r\n"
      "Content-Type: multipart/mixed; boundary=\"boundary2\"\r\n"
      "\r\n"
      "--boundary2\r\n"
      "Content-Disposition: file; filename=\"sample.txt\"\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "\r\n"
      "text line1\ntext line2\n\r\n"
      "--boundary2\r\n"
      "Content-Disposition: file; filename=\"test.bin\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "\r\n"
      "\x01\x02\x03\x04\x05\r\n"
      "--boundary2--\r\n"
      "--boundary1--";
  EXPECT_EQ(expected_data, all_data);
  EXPECT_EQ(all_data.size(), form_data.GetDataSize());
}
}  // namespace http
}  // namespace chromeos
