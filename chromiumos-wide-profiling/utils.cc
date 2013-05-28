// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/md5.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"

#include "utils.h"

namespace {

// Specify buffer size to be used to read files.
// This is allocated on the stack, so make sure it's less than 16k.
const int kFileReadSize = 1024;

// Trim leading and trailing whitespace from |str|.
void TrimWhitespace(string* str) {
  const char kWhitespaceCharacters[] = " \t\n\r";
  size_t end = str->find_last_not_of(kWhitespaceCharacters);
  if (end != std::string::npos)
    *str = str->substr(0, end+1);
  size_t start = str->find_first_not_of(kWhitespaceCharacters);
  if (start != std::string::npos)
    *str = str->substr(start);
}

// Perf report command and arguments.
// Don't attempt to symbolize:  --symfs=/dev/null
// Use stdio:                   --stdio
// Show event count:            -n
// Use subsequent input file:   -i
const char kPerfReportCommand[] =
    "/usr/sbin/perf report --symfs=/dev/null --stdio -n -i ";

// Given a perf report file, get the perf report and read it into a string.
bool GetPerfReport(const string& filename, string* output) {
  // Redirecting stderr does lose warnings and errors, but serious errors should
  // be caught by the return value of perf report.
  // TODO(sque): Use RunCommandAndGetOutput().
  string cmd = string(kPerfReportCommand) + filename + " 2>/dev/null";
  FILE* file = popen(cmd.c_str(), "r");
  if (!file) {
    LOG(ERROR) << "Could not execute '" << cmd << "'.";
    return false;
  }

  char buffer[kFileReadSize];
  // Read line by line, discarding commented lines.  That should skip the
  // header metadata, which we are not including.
  output->clear();
  while (fgets(buffer, sizeof(buffer), file)) {
    if (buffer[0] == '#')
      continue;
    string str = buffer;
    TrimWhitespace(&str);
    *output += str + '\n';
  }

  int status = pclose(file);
  if (status) {
    LOG(ERROR) << "Perf command: '" << cmd << "' returned status: " << status;
    return false;
  };

  return true;
}

}  // namespace

namespace quipper {

uint64 Md5Prefix(const string& input) {
  uint64 digest_prefix = 0;
  unsigned char digest[MD5_DIGEST_LENGTH + 1];

  MD5(reinterpret_cast<const unsigned char*>(input.c_str()), input.size(),
      digest);
  // We need 64-bits / # of bits in a byte.
  stringstream ss;
  for( size_t i = 0 ; i < sizeof(uint64) ; i++ )
    // The setw(2) and setfill('0') calls are needed to make sure we output 2
    // hex characters for every 8-bits of the hash.
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<unsigned int>(digest[i]);
  ss >> digest_prefix;
  return digest_prefix;
}

std::ifstream::pos_type GetFileSize(const string& filename) {
  std::ifstream in(filename.c_str(), std::ifstream::in | std::ifstream::binary);
  in.seekg(0, std::ifstream::end);
  return in.tellg();
}

bool BufferToFile(const string& filename,
                  const std::vector<char> & contents) {
  std::ofstream out(filename.c_str(), std::ios::binary);
  if (out.good())
  {
    out.write(&contents[0], contents.size() * sizeof(contents[0]));
    out.close();
    if (out.good())
      return true;
  }
  return false;
}

bool FileToBuffer(const string& filename, std::vector<char>* contents) {
  contents->reserve(GetFileSize(filename));
  std::ifstream in(filename.c_str(), std::ios::binary);
  if (in.good())
  {
    contents->assign(std::istreambuf_iterator<char>(in),
                     std::istreambuf_iterator<char>());
    if (in.good())
      return true;
  }
  return false;
}

bool CompareFileContents(const string& a, const string& b) {
  struct FileInfo {
    string name;
    std::vector<char> contents;
  };
  FileInfo file_infos[2];
  file_infos[0].name = a;
  file_infos[1].name = b;

  for ( size_t i = 0 ; i < sizeof(file_infos)/sizeof(file_infos[0]) ; i++ ) {
    if(!FileToBuffer(file_infos[i].name, &file_infos[i].contents))
      return false;
  }

  return file_infos[0].contents == file_infos[1].contents;
}

bool CreateNamedTempFile(string * name) {
  char filename[] = "/tmp/XXXXXX";
  int fd = mkstemp(filename);
  if (fd == -1)
    return false;
  close(fd);
  *name = filename;
  return true;
}

bool ComparePerfReports(const string& a, const string& b) {
  const string* files[] = { &a, &b };
  std::map<string, string> outputs;

  // Generate a perf report for each file.
  for (unsigned int i = 0; i < arraysize(files); ++i)
    CHECK(GetPerfReport(*files[i], &outputs[*files[i]]));

  // Compare the output strings.
  return outputs[a] == outputs[b];
}

uint64 AlignSize(uint64 size, uint32 align_size) {
  return ((size + align_size - 1) / align_size) * align_size;
}

bool ReadFileToData(const string& filename, std::vector<char>* data) {
  std::ifstream in(filename.c_str(), std::ios::binary);
  if (!in.good()) {
    LOG(ERROR) << "Failed to open file " << filename;
    return false;
  }
  in.seekg(0, in.end);
  size_t length = in.tellg();
  in.seekg(0, in.beg);
  data->resize(length);

  in.read(&(*data)[0], length);

  if (!in.good()) {
    LOG(ERROR) << "Error reading from file " << filename;
    return false;
  }
  return true;
}

bool WriteDataToFile(const std::vector<char>& data, const string& filename) {
  std::ofstream out(filename.c_str(), std::ios::binary);
  out.seekp(0, std::ios::beg);
  out.write(&data[0], data.size());
  return out.good();
}

bool RunCommandAndGetStdout(const string& command, std::vector<char>* output) {
  FILE* fp = popen(command.c_str(), "r");
  if (!fp)
    return false;

  output->clear();
  char buf[kFileReadSize];
  while (!feof(fp)) {
    size_t size_read = fread(buf, 1, sizeof(buf), fp);
    size_t prev_size = output->size();
    output->resize(prev_size + size_read);
    memcpy(&(*output)[prev_size], buf, size_read);
  }
  if (pclose(fp))
    return false;

  return true;
}

}  // namespace quipper
