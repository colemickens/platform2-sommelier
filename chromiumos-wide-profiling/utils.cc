// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/md5.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <istream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"

#include "utils.h"

namespace {

// Specify buffer size to be used to read the perf report.
const int kPerfReportReadBufferSize = 0x10000;

// Perf report command and arguments.
// Don't attempt to symbolize:  --symfs=/dev/null
// Use stdio:                   --stdio
// Show event count:            -n
// Use subsequent input file:   -i
const char kPerfReportCommand[] =
    "/usr/sbin/perf report --symfs=/dev/null --stdio -n -i ";

// Given a perf report file, get the perf report and read it into a string.
bool GetPerfReport(const string& filename, string* output) {
  string cmd = string(kPerfReportCommand) + filename;
  FILE* file = popen(cmd.c_str(), "r");
  if (!file) {
    LOG(ERROR) << "Could not execute '" << cmd << "'.";
    return false;
  }

  char buffer[kPerfReportReadBufferSize];
  // Read line by line, discarding commented lines.  That should skip the
  // header metadata, which we are not including.
  output->clear();
  while (fgets(buffer, sizeof(buffer), file)) {
    if (buffer[0] == '#')
      continue;
    string str;
    TrimWhitespaceASCII(buffer, TRIM_ALL, &str);
    *output += str + '\n';
  }
  fclose(file);

  return true;
}

// Given a general perf sample format |sample_type|, return the fields of that
// format that are present in a sample for an event of type |event_type|.
//
// e.g. FORK and EXIT events have the fields {time, pid/tid, cpu, id}.
// Given a sample type with fields {ip, time, pid/tid, and period}, return
// the intersection of these two field sets: {time, pid/tid}.
//
// All field formats are bitfields, as defined by enum perf_event_sample_format
// in kernel/perf_event.h.
uint64 GetSampleFieldsForEventType(uint32 event_type, uint64 sample_type) {
  uint64 mask = kuint64max;
  switch (event_type) {
  case PERF_RECORD_SAMPLE:
    // All fields of |sample_type| are present in an actual sample event.
    break;
  case PERF_RECORD_MMAP:
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ID |
           PERF_SAMPLE_CPU;
    break;
  case PERF_RECORD_FORK:
  case PERF_RECORD_EXIT:
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ID |
           PERF_SAMPLE_CPU;
    break;
  case PERF_RECORD_COMM:
    mask = PERF_SAMPLE_TID | PERF_SAMPLE_TIME | PERF_SAMPLE_ID |
           PERF_SAMPLE_CPU;
    break;
  // Not currently processing these events.
  case PERF_RECORD_LOST:
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
  case PERF_RECORD_READ:
  case PERF_RECORD_MAX:
    break;
  default:
    NOTREACHED() << "Unknown event type " << event_type;
  }
  return sample_type & mask;
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
// This function returns the length of the 8-byte-aligned for storing |string|.
size_t GetUint64AlignedStringLength(const char* string) {
  return (strlen(string) / sizeof(uint64) + 1) * sizeof(uint64);
}

// Returns the offset in bytes within a perf event structure at which the raw
// perf sample data is located.
uint64 GetPerfSampleDataOffset(const event_t& event) {
  uint64 offset = kuint64max;
  switch (event.header.type) {
  case PERF_RECORD_SAMPLE:
    offset = sizeof(event.header);
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
  case PERF_RECORD_THROTTLE:
  case PERF_RECORD_UNTHROTTLE:
  case PERF_RECORD_READ:
  case PERF_RECORD_MAX:
    NOTREACHED() << "Unsupported event type " << event.header.type;
  default:
    NOTREACHED() << "Unknown event type " << event.header.type;
  }
  // Make sure the offset was valid
  CHECK_NE(offset, kuint64max);
  CHECK_EQ(offset % sizeof(uint64), 0U);
  return offset;
}

size_t ReadPerfSampleFromData(const uint64* array,
                              const uint64 sample_fields,
                              struct perf_sample* sample) {
  size_t num_values_read = 0;
  for (int index = 0; (sample_fields >> index) > 0; ++index) {
    uint64 sample_type = (1 << index);
    union {
      uint32 val32[sizeof(uint64) / sizeof(uint32)];
      uint64 val64;
    };
    if (!(sample_type & sample_fields))
      continue;

    val64 = *array++;
    ++num_values_read;
    switch (sample_type) {
    case PERF_SAMPLE_IP:
      sample->ip = val64;
      break;
    case PERF_SAMPLE_TID:
      // TODO(sque): might have to check for endianness.
      sample->pid = val32[0];
      sample->tid = val32[1];
      break;
    case PERF_SAMPLE_TIME:
      sample->time = val64;
      break;
    case PERF_SAMPLE_ADDR:
      sample->addr = val64;
      break;
    case PERF_SAMPLE_ID:
      sample->id = val64;
      break;
    case PERF_SAMPLE_STREAM_ID:
      sample->stream_id = val64;
      break;
    case PERF_SAMPLE_CPU:
      sample->cpu = val32[0];
      break;
    case PERF_SAMPLE_PERIOD:
      sample->period = val64;
      break;
    default:
      NOTREACHED() << "Invalid sample type " << (void*)sample_type;
    }
  }
  return num_values_read * sizeof(uint64);
}

size_t WritePerfSampleToData(const struct perf_sample& sample,
                             const uint64 sample_fields,
                             uint64* array) {
  size_t num_values_written = 0;
  for (int index = 0; (sample_fields >> index) > 0; ++index) {
    uint64 sample_type = (1 << index);
    union {
      uint32 val32[sizeof(uint64) / sizeof(uint32)];
      uint64 val64;
    };
    if (!(sample_type & sample_fields))
      continue;

    switch (sample_type) {
    case PERF_SAMPLE_IP:
      val64 = sample.ip;
      break;
    case PERF_SAMPLE_TID:
      val32[0] = sample.pid;
      val32[1] = sample.tid;
      break;
    case PERF_SAMPLE_TIME:
      val64 = sample.time;
      break;
    case PERF_SAMPLE_ADDR:
      val64 = sample.addr;
      break;
    case PERF_SAMPLE_ID:
      val64 = sample.id;
      break;
    case PERF_SAMPLE_STREAM_ID:
      val64 = sample.stream_id;
      break;
    case PERF_SAMPLE_CPU:
      val64 = sample.cpu;
      break;
    case PERF_SAMPLE_PERIOD:
      val64 = sample.period;
      break;
    default:
      NOTREACHED() << "Invalid sample type " << (void*)sample_type;
    }
    *array++ = val64;
    ++num_values_written;
  }
  return num_values_written * sizeof(uint64);
}

}  // namespace

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

bool ReadPerfSampleInfo(const event_t& event,
                        uint64 sample_type,
                        struct perf_sample* sample) {
  CHECK(sample);

  uint64 sample_format = GetSampleFieldsForEventType(event.header.type,
                                                     sample_type);
  uint64 offset = GetPerfSampleDataOffset(event);
  memset(sample, 0, sizeof(*sample));
  size_t size_read = ReadPerfSampleFromData(
      reinterpret_cast<const uint64*>(&event) + offset / sizeof(uint64),
      sample_format,
      sample);

  size_t expected_size = event.header.size - offset;
  return (size_read == expected_size);
}

// Requires that event->header has already been filled with the sample data.
bool WritePerfSampleInfo(const perf_sample& sample,
                         uint64 sample_type,
                         event_t* event) {
  CHECK(event);

  uint64 sample_format = GetSampleFieldsForEventType(event->header.type,
                                                     sample_type);
  uint64 offset = GetPerfSampleDataOffset(*event);

  size_t expected_size = event->header.size - offset;
  memset(reinterpret_cast<uint8*>(event) + offset, 0, expected_size);
  size_t size_written = WritePerfSampleToData(
      sample,
      sample_format,
      reinterpret_cast<uint64*>(event) + offset / sizeof(uint64));

  return (size_written == expected_size);
}
