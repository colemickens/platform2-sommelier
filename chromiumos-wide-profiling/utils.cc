// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <openssl/md5.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
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
#include "base/stringprintf.h"

#include "utils.h"

namespace {

// Specify buffer size to be used to read files.
// This is allocated on the stack, so make sure it's less than 16k.
const int kFileReadSize = 1024;

// Newline character.
const char kNewLineDelimiter = '\n';

// Number of hex digits in a byte.
const int kNumHexDigitsInByte = 2;

// Trim leading and trailing whitespace from |str|.
void TrimWhitespace(string* str) {
  const char kWhitespaceCharacters[] = " \t\n\r";
  size_t end = str->find_last_not_of(kWhitespaceCharacters);
  if (end != std::string::npos) {
    size_t start = str->find_first_not_of(kWhitespaceCharacters);
    *str = str->substr(start, end + 1 - start);
  } else {
    // The string contains only whitespace.
    *str = "";
  }
}

enum PerfDataType {
  kPerfDataNormal,    // Perf data is in normal format.
  kPerfDataPiped,     // Perf data is in piped format.
};

string GetPerfReportArgs(PerfDataType data_type, const string& sort_fields) {
  // The marker in the command strings where custom sort fields can be inserted.
  const char kSortFieldsPlaceholder[] = "[SORT_FIELDS]";

  // List of basic arguments for perf report.
  const char* kPerfReportArgs[] = {
    "report",                     // Tells perf to generate a perf report.
    "--symfs=/dev/null",          // Don't attempt to symbolize.
    "--stdio",                    // Output to stdio.
    "--sort",                     // Specify fields by which to sort.
    kSortFieldsPlaceholder,       // Value of previous arg, listing sort fields.
    "-t ,",                       // Use comma as a separator.
    "-n",                         // Show event count.
    "-I",                         // Show topology metadata.
    "-i",                         // Use subsequent input file.
  };

  // Append this to the command for piped data.
  const char kPipedReportSuffix[] = "- < ";

  // Construct the argument string by concatenating the arguments.
  string args;
  for (size_t i = 0; i < arraysize(kPerfReportArgs); ++i)
    args += string(kPerfReportArgs[i]) + " ";

  args.replace(args.find(kSortFieldsPlaceholder),
               strlen(kSortFieldsPlaceholder),
               sort_fields);

  switch(data_type) {
  case kPerfDataNormal:
    // Do nothing.
    break;
  case kPerfDataPiped:
    args += kPipedReportSuffix;
    break;
  default:
    LOG(ERROR) << "Invalid perf data type: " << data_type;
    break;
  }
  return args;
}

string GetPerfCommandString(const string& args, const string& filename) {
  // Construct the commands.
  string command = string(quipper::kPerfPath) + " " + args;

  // Redirecting stderr does lose warnings and errors, but serious errors should
  // be caught by the return value of perf report.
  command += filename + " 2>/dev/null";

  return command;
}

// By default, sort normal files by command, DSO name, and symbol/address.
const char kDefaultSortFields[] = "comm,dso,sym";

// By default, sort piped files by command and DSO name.
const char kDefaultPipedSortFields[] = "comm,dso";

// The piped commands above produce comma-separated lines with the following
// fields:
enum {
  PERF_REPORT_OVERHEAD,
  PERF_REPORT_SAMPLES,
  PERF_REPORT_COMMAND,
  PERF_REPORT_SHARED_OBJECT,
  NUM_PERF_REPORT_FIELDS,
};

const char kPerfBuildIDArgs[] = "buildid-list -i ";

const int kPerfReportParseError = -1;

const char kUnknownDSOString[] = "[unknown]";

const int kUninitializedUnknownValue = -1;

// Tolerance for equality comparison in CompareMapsAccountingForUnknownEntries.
const double kPerfReportEntryErrorThreshold = 0.05;

const char kPerfReportCommentCharacter = '#';
const char kPerfReportMetadataFieldCharacter = ':';

void SeparateLines(const std::vector<char>& bytes, std::vector<string>* lines) {
  // TODO(rohinmshah): Use something from base.
  std::vector<char>::const_iterator start = bytes.begin();
  std::vector<char>::const_iterator end =
      std::find(start, bytes.end(), kNewLineDelimiter);
  while (end != bytes.end()) {
    lines->push_back(string(start, end));
    start = end + 1;
    end = std::find(start, bytes.end(), kNewLineDelimiter);
  }
}

// Given a perf piped data file, get the perf report and read it into a string.
// is_normal_mode should be true if the INPUT file to quipper was in normal
// mode.  Note that a file written by quipper is always in normal mode.
bool GetPerfReport(const string& filename,
                   std::vector<string>* output,
                   string sort_fields,
                   bool is_normal_mode) {
  string cmd =
      GetPerfCommandString(
          GetPerfReportArgs(is_normal_mode ? kPerfDataNormal : kPerfDataPiped,
                            sort_fields),
          filename);
  std::vector<char> stdout;
  if (!quipper::RunCommandAndGetStdout(cmd, &stdout))
    return false;
  std::vector<string> lines;
  SeparateLines(stdout, &lines);

  // Read line by line, discarding commented lines.
  // Only keep commented lines of the form
  // # <supported metadata> :
  // Where <supported metadata> is any string in kSupportedMetadata.
  output->clear();
  for (size_t i = 0; i < lines.size(); ++i) {
    string line = lines[i];
    if (line.empty()) {
      output->push_back(line);
      continue;
    }
    bool use_line = false;
    if (line[0] != kPerfReportCommentCharacter)
      use_line = true;

    for (size_t j = 0; quipper::kSupportedMetadata[j]; ++j) {
      string valid_prefix = StringPrintf("%c %s",
                                         kPerfReportCommentCharacter,
                                         quipper::kSupportedMetadata[j]);
      if (line.substr(0, valid_prefix.size()) == valid_prefix)
        use_line = true;
    }

    if (use_line) {
      TrimWhitespace(&line);
      output->push_back(line);
    }
  }

  return true;
}

// Populates the map using information from the report.
// Returns the index at which the next section begins, or kPerfReportParseError
// to signal an error.
// The report is expected to contain lines in the format
// Overhead,Samples,Command,Shared Object
// And the section ends with the empty string.
int ParsePerfReportSection(const std::vector<string>& report, size_t index,
                           std::map<string, double>* dso_to_overhead,
                           std::map<string, int>* dso_to_num_samples) {
  dso_to_overhead->clear();
  dso_to_num_samples->clear();
  while (index < report.size() && !report[index].empty()) {
    const string& item = report[index++];
    std::stringstream ss(item);
    std::vector<string> tokens;
    std::string token;
    while (std::getline(ss, token, ','))
      tokens.push_back(token);

    if (tokens.size() != NUM_PERF_REPORT_FIELDS)
      return kPerfReportParseError;

    string key = tokens[PERF_REPORT_COMMAND] + "+"
        + tokens[PERF_REPORT_SHARED_OBJECT];
    double overhead = atof(tokens[PERF_REPORT_OVERHEAD].c_str());
    int num_samples = atoi(tokens[PERF_REPORT_SAMPLES].c_str());

    if (num_samples == 0)
      return kPerfReportParseError;

    CHECK(dso_to_overhead->find(key) == dso_to_overhead->end())
        << "Command + Shared Object " << key << " occurred twice in a section";
    dso_to_overhead->insert(std::pair<string, double>(key, overhead));
    dso_to_num_samples->insert(std::pair<string, int>(key, num_samples));
  }

  // Skip any more empty lines.
  while (index < report.size() && report[index].empty())
    ++index;

  return index;
}

// Compares two maps created by ParsePerfReportSection.
// The input map may contain kUnknownDSOString, but the output map should not.
// class T is used to support ints and doubles.
// Checks the following conditions:
// 1. No key in output_map has a substring kUnknownDSOString.
// 2. Every key in input_map without the kUnknownDSOString substring is also
//    present in output_map.
// 3. The values in input_map and output_map agree with each other.
template <class T>
bool CompareMapsAccountingForUnknownEntries(
    const std::map<string, T>& input_map,
    const std::map<string, T>& output_map) {
  T unknown_value = kUninitializedUnknownValue;
  T output_minus_input = 0;
  typename std::map<string, T>::const_iterator it;

  for (it = input_map.begin(); it != input_map.end(); ++it) {
    string key = it->first;
    if (key.find(kUnknownDSOString) != string::npos) {
      CHECK_EQ(unknown_value, kUninitializedUnknownValue);
      unknown_value = it->second;
    } else if (output_map.find(key) == output_map.end()) {
      return false;
    } else {
      output_minus_input += (output_map.at(key) - it->second);
    }
  }

  // Add any items present in output_map but not input_map.
  for (it = output_map.begin(); it != output_map.end(); ++it) {
    string key = it->first;
    if (key.find(kUnknownDSOString) != string::npos)
      return false;
    else if (input_map.find(key) == input_map.end())
      output_minus_input += it->second;
  }

  // If there were no unknown samples, don't use the error threshold,
  // because in this case the reports should be identical.
  if (unknown_value == kUninitializedUnknownValue)
    return output_minus_input == 0;

  T difference = output_minus_input - unknown_value;
  return (difference < kPerfReportEntryErrorThreshold) &&
         (difference > -kPerfReportEntryErrorThreshold);
}

// Given a valid open file handle |fp|, returns the size of the file.
long int GetFileSizeFromHandle(FILE* fp) {
  long int position = ftell(fp);
  fseek(fp, 0, SEEK_END);
  long int file_size = ftell(fp);
  // Restore the original file handle position.
  fseek(fp, position, SEEK_SET);
  return file_size;
}

}  // namespace

namespace quipper {

const char* kSupportedMetadata[] = {
  "hostname",
  "os release",
  "perf version",
  "arch",
  "nrcpus online",
  "nrcpus avail",
  "cpudesc",
  "cpuid",
  "total memory",
  "cmdline",
  "event",
  "sibling cores",        // CPU topology.
  "sibling threads",      // CPU topology.
  "node0 meminfo",        // NUMA topology.
  "node0 cpu list",       // NUMA topology.
  "node1 meminfo",        // NUMA topology.
  "node1 cpu list",       // NUMA topology.
  NULL,
};

namespace {

// Returns the number of lines at the beginning of |report| containing metadata.
// Stores the metadata types found in |report| in |*seen_metadata|.
// Each string in |report| is a line of the report.
// TODO(sque): This function is huge.  It should be refactored into smaller
// helper functions.
int CountReportMetadata(const std::vector<string>& report,
                        std::map<string, string>* seen_metadata) {
  size_t index = 0;
  while (index < report.size()) {
    const string& line = report[index];
    if (line[0] != kPerfReportCommentCharacter)
      break;
    size_t index_of_colon = line.find(kPerfReportMetadataFieldCharacter);
    if (index_of_colon == string::npos)
      return -1;

    // Get the metadata type name.
    string key = line.substr(1, index_of_colon - 1);
    TrimWhitespace(&key);

    bool metadata_is_supported = false;
    for (size_t i = 0; i < arraysize(kSupportedMetadata); ++i) {
      if (key == kSupportedMetadata[i]) {
        metadata_is_supported = true;
        break;
      }
    }

    // The field should have only ASCII printable characters.  The opposite of
    // printable characters are control charaters.
    if (std::find_if(key.begin(), key.end(), iscntrl) != key.end())
      return -1;

    // Add the metadata to the set of seen metadata.
    if (seen_metadata && metadata_is_supported) {
      string value = line.substr(index_of_colon + 1, string::npos);
      TrimWhitespace(&value);
      (*seen_metadata)[key] = value;
    }

    ++index;
  }
  return index;
}

}  // namespace

string GetTestInputFilePath(const string& filename) {
  return string(kPerfDataInputPath) + filename;
}

event_t* CallocMemoryForEvent(size_t size) {
  event_t* event = reinterpret_cast<event_t*>(calloc(1, size));
  CHECK(event);
  return event;
}

build_id_event* CallocMemoryForBuildID(size_t size) {
  build_id_event* event = reinterpret_cast<build_id_event*>(calloc(1, size));
  CHECK(event);
  return event;
}

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

long int GetFileSize(const string& filename) {
  FILE* fp = fopen(filename.c_str(), "rb");
  if (!fp)
    return -1;
  long int file_size = GetFileSizeFromHandle(fp);
  fclose(fp);
  return file_size;
}

bool BufferToFile(const string& filename, const std::vector<char>& contents) {
  FILE* fp = fopen(filename.c_str(), "wb");
  if (!fp)
    return false;
  // Do not write anything if |contents| contains nothing.  fopen will create
  // an empty file.
  if (!contents.empty()) {
    CHECK_GT(fwrite(&contents[0], contents.size() * sizeof(contents[0]), 1, fp),
             0U);
  }
  fclose(fp);
  return true;
}

bool FileToBuffer(const string& filename, std::vector<char>* contents) {
  FILE* fp = fopen(filename.c_str(), "rb");
  if (!fp)
    return false;
  long int file_size = GetFileSizeFromHandle(fp);
  contents->resize(file_size);
  // Do not read anything if the file exists but is empty.
  if (file_size > 0)
    CHECK_GT(fread(&(*contents)[0], file_size, 1, fp), 0U);
  fclose(fp);
  return true;
}

bool CompareFileContents(const string& file1, const string& file2) {
  struct FileInfo {
    string name;
    std::vector<char> contents;
  };
  FileInfo file_infos[2];
  file_infos[0].name = file1;
  file_infos[1].name = file2;

  for ( size_t i = 0 ; i < sizeof(file_infos)/sizeof(file_infos[0]) ; i++ ) {
    if(!FileToBuffer(file_infos[i].name, &file_infos[i].contents))
      return false;
  }

  return file_infos[0].contents == file_infos[1].contents;
}

ScopedTempFile::ScopedTempFile() {
  char filename[] = "/tmp/XXXXXX";
  int fd = mkstemp(filename);
  if (fd == -1)
    return;
  close(fd);
  path_ = filename;
}

ScopedTempDir::ScopedTempDir() {
  char dirname[] = "/tmp/XXXXXX";
  const char* name = mkdtemp(dirname);
  if (!name)
    return;
  path_ = string(name) + "/";
}

ScopedTempPath::~ScopedTempPath() {
  if (!path_.empty() && remove(path_.c_str()))
    LOG(ERROR) << "Error while removing " << path_ << ", errno: " << errno;
}

bool ComparePerfReports(const string& quipper_input,
                        const string& quipper_output) {
  return ComparePerfReportsByFields(quipper_input,
                                    quipper_output,
                                    kDefaultSortFields);
}

bool ComparePerfReportsByFields(const string& quipper_input,
                                const string& quipper_output,
                                const string& sort_fields) {
  // Generate a perf report for each file.
  std::vector<string> quipper_input_report, quipper_output_report;
  CHECK(GetPerfReport(quipper_input, &quipper_input_report,
                      sort_fields, true));
  CHECK(GetPerfReport(quipper_output, &quipper_output_report,
                      sort_fields, true));
  int input_size = quipper_input_report.size();
  int output_size = quipper_output_report.size();

  // Compare the output strings.
  if (input_size != output_size)
    return false;

  for (int i = 0; i < input_size; i++) {
    if (quipper_input_report[i] != quipper_output_report[i])
      return false;
  }

  return true;
}

bool ComparePipedPerfReports(const string& quipper_input,
                             const string& quipper_output,
                             std::map<string, string>* seen_metadata) {
  // Generate a perf report for each file.
  std::vector<string> quipper_input_report, quipper_output_report;
  CHECK(GetPerfReport(quipper_input, &quipper_input_report,
                      kDefaultPipedSortFields, false));
  CHECK(GetPerfReport(quipper_output, &quipper_output_report,
                      kDefaultPipedSortFields, true));
  int input_size = quipper_input_report.size();
  int output_size = quipper_output_report.size();

  // TODO(sque): The chromiumos perf tool does not show metadata for piped data,
  // but other perf tools might.  We should check that the metadata values match
  // when both the input and output reports have metadata.
  int input_index = CountReportMetadata(quipper_input_report, NULL);
  int output_index = CountReportMetadata(quipper_output_report, seen_metadata);

  if (input_index < 0)
    return false;
  if (output_index < 0)
    return false;

  // Parse each section of the perf report and make sure they agree.
  // See ParsePerfReportSection and CompareMapsAccountingForUnknownEntries.

  while (input_index < input_size && output_index < output_size) {
    std::map<string, double> input_overhead, output_overhead;
    std::map<string, int> input_num_samples, output_num_samples;

    input_index = ParsePerfReportSection(quipper_input_report, input_index,
                                         &input_overhead, &input_num_samples);
    if (input_index == kPerfReportParseError)
      return false;
    output_index = ParsePerfReportSection(quipper_output_report, output_index,
                                          &output_overhead,
                                          &output_num_samples);
    if (output_index == kPerfReportParseError)
      return false;

    if (!CompareMapsAccountingForUnknownEntries(input_overhead,
                                                output_overhead))
      return false;
    if (!CompareMapsAccountingForUnknownEntries(input_num_samples,
                                                output_num_samples))
      return false;
  }

  return (input_index == input_size) && (output_index == output_size);
}

bool GetPerfBuildIDMap(const string& filename,
                       std::map<string, string>* output) {
  string cmd = GetPerfCommandString(kPerfBuildIDArgs, filename);
  std::vector<char> stdout;
  if (!quipper::RunCommandAndGetStdout(cmd, &stdout))
    return false;
  std::vector<string> lines;
  SeparateLines(stdout, &lines);

  /* The output now looks like the following:
     cff4586f322eb113d59f54f6e0312767c6746524 [kernel.kallsyms]
     c099914666223ff6403882604c96803f180688f5 /lib64/libc-2.15.so
     7ac2d19f88118a4970adb48a84ed897b963e3fb7 /lib64/libpthread-2.15.so
  */
  output->clear();
  for (size_t i = 0; i < lines.size(); ++i) {
    string line = lines[i];
    TrimWhitespace(&line);
    size_t separator = line.find(' ');
    string build_id = line.substr(0, separator);
    string filename = line.substr(separator + 1);
    (*output)[filename] = build_id;
  }

  return true;
}

bool ComparePerfBuildIDLists(const string& file1, const string& file2) {
  const string* files[] = { &file1, &file2 };
  std::map<string, std::map<string, string> > outputs;

  // Generate a build id list for each file.
  for (unsigned int i = 0; i < arraysize(files); ++i)
    CHECK(GetPerfBuildIDMap(*files[i], &outputs[*files[i]]));

  // Compare the output strings.
  return outputs[file1] == outputs[file2];
}

string HexToString(const u8* array, size_t length) {
  // Convert the bytes to hex digits one at a time.
  // There will be kNumHexDigitsInByte hex digits, and 1 char for NUL.
  char buffer[kNumHexDigitsInByte + 1];
  string result = "";
  for (size_t i = 0; i < length; ++i) {
    snprintf(buffer, sizeof(buffer), "%02x", array[i]);
    result += buffer;
  }
  return result;
}

bool StringToHex(const string& str, u8* array, size_t length) {
  const int kHexRadix = 16;
  char* err;
  // Loop through kNumHexDigitsInByte characters at a time (to get one byte)
  // Stop when there are no more characters, or the array has been filled.
  for (size_t i = 0;
       (i + 1) * kNumHexDigitsInByte <= str.size() && i < length;
       ++i) {
    string one_byte = str.substr(i * kNumHexDigitsInByte, kNumHexDigitsInByte);
    array[i] = strtol(one_byte.c_str(), &err, kHexRadix);
    if (*err)
      return false;
  }
  return true;
}

uint64 AlignSize(uint64 size, uint32 align_size) {
  return ((size + align_size - 1) / align_size) * align_size;
}

// In perf data, strings are packed into the smallest number of 8-byte blocks
// possible, including the null terminator.
// e.g.
//    "0123"                ->  5 bytes -> packed into  8 bytes
//    "0123456"             ->  8 bytes -> packed into  8 bytes
//    "01234567"            ->  9 bytes -> packed into 16 bytes
//    "0123456789abcd"      -> 15 bytes -> packed into 16 bytes
//    "0123456789abcde"     -> 16 bytes -> packed into 16 bytes
//    "0123456789abcdef"    -> 17 bytes -> packed into 24 bytes
//
// Returns the size of the 8-byte-aligned memory for storing |string|.
size_t GetUint64AlignedStringLength(const string& str) {
  return AlignSize(str.size() + 1, sizeof(uint64));
}

uint64 GetSampleFieldsForEventType(uint32 event_type, uint64 sample_type) {
  uint64 mask = kuint64max;
  switch (event_type) {
  case PERF_RECORD_SAMPLE:
    // IP and pid/tid fields of sample events are read as part of event_t, so
    // mask away those two fields.
    mask = ~(PERF_SAMPLE_IP | PERF_SAMPLE_TID);
    break;
  case PERF_RECORD_MMAP:
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
  case PERF_RECORD_COMM:
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ID |
           PERF_SAMPLE_CPU;
    break;
  // Not currently processing these events.
  case PERF_RECORD_LOST:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_CPU;
    break;
  case PERF_RECORD_READ:
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event_type;
  }
  return sample_type & mask;
}

uint64 GetPerfSampleDataOffset(const event_t& event) {
  uint64 offset = kuint64max;
  switch (event.header.type) {
  case PERF_RECORD_SAMPLE:
    offset = sizeof(event.ip);
    break;
  case PERF_RECORD_MMAP:
    offset = sizeof(event.mmap) - sizeof(event.mmap.filename) +
             GetUint64AlignedStringLength(event.mmap.filename);
    break;
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
    offset = sizeof(event.fork);
    break;
  case PERF_RECORD_COMM:
    offset = sizeof(event.comm) - sizeof(event.comm.comm) +
             GetUint64AlignedStringLength(event.comm.comm);
    break;
  case PERF_RECORD_LOST:
    offset = sizeof(event.lost);
    break;
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
    offset = sizeof(event.throttle);
    break;
  case PERF_RECORD_READ:
    offset = sizeof(event.read);
    break;
  default:
    LOG(FATAL) << "Unknown event type " << event.header.type;
    break;
  }
  // Make sure the offset was valid
  CHECK_NE(offset, kuint64max);
  CHECK_EQ(offset % sizeof(uint64), 0U);
  return offset;
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
