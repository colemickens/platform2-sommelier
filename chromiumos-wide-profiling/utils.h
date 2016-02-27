// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMIUMOS_WIDE_PROFILING_UTILS_H_
#define CHROMIUMOS_WIDE_PROFILING_UTILS_H_

#include <byteswap.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>  // for free()

#include <bitset>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "base/logging.h"

#include "chromiumos-wide-profiling/compat/string.h"
#include "chromiumos-wide-profiling/kernel/perf_internals.h"

namespace quipper {

class PerfDataProto_PerfEvent;
class PerfDataProto_SampleInfo;

struct FreeDeleter {
  inline void operator()(void* pointer) {
    free(pointer);
  }
};

template <typename T>
using malloced_unique_ptr = std::unique_ptr<T, FreeDeleter>;

// Given a valid open file handle |fp|, returns the size of the file.
int64_t GetFileSizeFromHandle(FILE* fp);

event_t* CallocMemoryForEvent(size_t size);
event_t* ReallocMemoryForEvent(event_t* event, size_t new_size);

build_id_event* CallocMemoryForBuildID(size_t size);

bool FileToBuffer(const string& filename, std::vector<char>* contents);

template <typename CharContainer>
bool BufferToFile(const string& filename, const CharContainer& contents) {
  FILE* fp = fopen(filename.c_str(), "wb");
  if (!fp)
    return false;
  // Do not write anything if |contents| contains nothing.  fopen will create
  // an empty file.
  if (!contents.empty()) {
    CHECK_EQ(fwrite(contents.data(),
                    sizeof(typename CharContainer::value_type),
                    contents.size(),
                    fp),
             contents.size());
  }
  fclose(fp);
  return true;
}

// Swaps the byte order of 16-bit, 32-bit, and 64-bit unsigned integers.
template <class T>
void ByteSwap(T* input) {
  switch (sizeof(T)) {
  case sizeof(uint8_t):
    LOG(WARNING) << "Attempting to byte swap on a single byte.";
    break;
  case sizeof(uint16_t):
    *input = bswap_16(*input);
    break;
  case sizeof(uint32_t):
    *input = bswap_32(*input);
    break;
  case sizeof(uint64_t):
    *input = bswap_64(*input);
    break;
  default:
    LOG(FATAL) << "Invalid size for byte swap: " << sizeof(T) << " bytes";
    break;
  }
}

// Swaps byte order of |value| if the |swap| flag is set. This function is
// trivial but it avoids filling code with "if (swap) { ... } " statements.
template <typename T>
T MaybeSwap(T value, bool swap) {
  if (swap)
    ByteSwap(&value);
  return value;
}

// Returns the number of bits in a numerical value.
template <typename T>
size_t GetNumBits(const T& value) {
  return std::bitset<sizeof(T) * CHAR_BIT>(value).count();
}

uint64_t Md5Prefix(const string& input);
uint64_t Md5Prefix(const std::vector<char>& input);

// Returns a string that represents |array| in hexadecimal.
string RawDataToHexString(const u8* array, size_t length);

// Given raw data in |str|, returns a string that represents the binary data as
// hexadecimal.
string RawDataToHexString(const string& str);

// Given a string |str| containing data represented in hexadecimal, converts to
// to raw bytes stored in |array|.  Returns true on success.  Only stores up to
// |length| bytes - if there are more characters in the string, they are
// ignored (but the function may still return true).
bool HexStringToRawData(const string& str, u8* array, size_t length);

// Round |value| up to the next |alignment|. I.e. returns the smallest multiple
// of |alignment| less than or equal to |value|. |alignment| must be a power
// of 2 (compile-time enforced).
template<unsigned int alignment,
         typename std::enable_if<
             alignment != 0 && (alignment&(alignment-1)) == 0
         >::type* = nullptr>
inline uint64_t Align(uint64_t value) {
  constexpr uint64_t mask = alignment - 1;
  return (value + mask) & ~mask;
}

// Allows passing a type parameter instead of a size.
template<typename T>
inline uint64_t Align(uint64_t value) {
  return Align<sizeof(T)>(value);
}

// In perf data, strings are packed into the smallest number of 8-byte blocks
// possible, including a null terminator.
// e.g.
//    "0123"                ->  5 bytes -> packed into  8 bytes
//    "0123456"             ->  8 bytes -> packed into  8 bytes
//    "01234567"            ->  9 bytes -> packed into 16 bytes
//    "0123456789abcd"      -> 15 bytes -> packed into 16 bytes
//    "0123456789abcde"     -> 16 bytes -> packed into 16 bytes
//    "0123456789abcdef"    -> 17 bytes -> packed into 24 bytes
//
// Returns the size of the 8-byte-aligned memory for storing |string|.
static inline size_t GetUint64AlignedStringLength(const string& str) {
  return Align<uint64_t>(str.size() + 1);
}

// Returns true iff the file exists.
bool FileExists(const string& filename);

// Reads the contents of a file into |data|.  Returns true on success, false if
// it fails.
bool ReadFileToData(const string& filename, std::vector<char>* data);

// Writes contents of |data| to a file with name |filename|, overwriting any
// existing file.  Returns true on success, false if it fails.
bool WriteDataToFile(const std::vector<char>& data, const string& filename);

// Trim leading and trailing whitespace from |str|.
void TrimWhitespace(string* str);

// Splits a character array by |delimiter| into a vector of strings tokens.
void SplitString(const string& str,
                 char delimiter,
                 std::vector<string>* tokens);

// If |event| is not of type PERF_RECORD_SAMPLE, returns the SampleInfo field
// within it. Otherwise returns nullptr.
const PerfDataProto_SampleInfo* GetSampleInfoForEvent(
    const PerfDataProto_PerfEvent& event);

// Returns the correct |sample_time_ns| field of a PerfEvent.
uint64_t GetTimeFromPerfEvent(const PerfDataProto_PerfEvent& event);

}  // namespace quipper

#endif  // CHROMIUMOS_WIDE_PROFILING_UTILS_H_
