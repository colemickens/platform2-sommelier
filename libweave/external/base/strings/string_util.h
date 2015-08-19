// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef BASE_STRINGS_STRING_UTIL_H_
#define BASE_STRINGS_STRING_UTIL_H_

#include <ctype.h>
#include <stdarg.h>   // va_list

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"  // For implicit conversions.

// On Android, bionic's stdio.h defines an snprintf macro when being built with
// clang. Undefine it here so it won't collide with base::snprintf().
#undef snprintf

namespace base {

// C standard-library functions like "strncasecmp" and "snprintf" that aren't
// cross-platform are provided as "base::strncasecmp", and their prototypes
// are listed below.  These functions are then implemented as inline calls
// to the platform-specific equivalents in the platform-specific headers.

// Wrapper for vsnprintf that always null-terminates and always returns the
// number of characters that would be in an untruncated formatted
// string, even when truncation occurs.
int vsnprintf(char* buffer, size_t size, const char* format, va_list arguments)
    PRINTF_FORMAT(3, 0);

// Some of these implementations need to be inlined.

// We separate the declaration from the implementation of this inline
// function just so the PRINTF_FORMAT works.
inline int snprintf(char* buffer, size_t size, const char* format, ...)
    PRINTF_FORMAT(3, 4);
inline int snprintf(char* buffer, size_t size, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

// Contains the set of characters representing whitespace in the corresponding
// encoding. Null-terminated. The ASCII versions are the whitespaces as defined
// by HTML5, and don't include control characters.
BASE_EXPORT extern const char kWhitespaceASCII[];

// Replaces characters in |replace_chars| from anywhere in |input| with
// |replace_with|.  Each character in |replace_chars| will be replaced with
// the |replace_with| string.  Returns true if any characters were replaced.
// |replace_chars| must be null-terminated.
// NOTE: Safe to use the same variable for both |input| and |output|.
BASE_EXPORT bool ReplaceChars(const std::string& input,
                              const base::StringPiece& replace_chars,
                              const std::string& replace_with,
                              std::string* output);

enum TrimPositions {
  TRIM_NONE     = 0,
  TRIM_LEADING  = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};

// Removes characters in |trim_chars| from the beginning and end of |input|.
// The 8-bit version only works on 8-bit characters, not UTF-8.
//
// It is safe to use the same variable for both |input| and |output| (this is
// the normal usage to trim in-place).
BASE_EXPORT bool TrimString(const std::string& input,
                            base::StringPiece trim_chars,
                            std::string* output);

// StringPiece versions of the above. The returned pieces refer to the original
// buffer.
BASE_EXPORT StringPiece TrimString(StringPiece input,
                                   const base::StringPiece& trim_chars,
                                   TrimPositions positions);

// Trims any whitespace from either end of the input string.  Returns where
// whitespace was found.
// The non-wide version has two functions:
// * TrimWhitespaceASCII()
//   This function is for ASCII strings and only looks for ASCII whitespace;
// Please choose the best one according to your usage.
// NOTE: Safe to use the same variable for both input and output.
BASE_EXPORT TrimPositions TrimWhitespaceASCII(const std::string& input,
                                              TrimPositions positions,
                                              std::string* output);

// Deprecated. This function is only for backward compatibility and calls
// TrimWhitespaceASCII().
BASE_EXPORT TrimPositions TrimWhitespace(const std::string& input,
                                         TrimPositions positions,
                                         std::string* output);

// Returns true if the specified string matches the criteria. How can a wide
// string be 8-bit or UTF8? It contains only characters that are < 256 (in the
// first case) or characters that use only 8-bits and whose 8-bit
// representation looks like a UTF-8 string (the second case).
//
// Note that IsStringUTF8 checks not only if the input is structurally
// valid but also if it doesn't contain any non-character codepoint
// (e.g. U+FFFE). It's done on purpose because all the existing callers want
// to have the maximum 'discriminating' power from other encodings. If
// there's a use case for just checking the structural validity, we have to
// add a new function for that.
//
// IsStringASCII assumes the input is likely all ASCII, and does not leave early
// if it is not the case.
BASE_EXPORT bool IsStringUTF8(const StringPiece& str);
BASE_EXPORT bool IsStringASCII(const StringPiece& str);

}  // namespace base

#if defined(OS_WIN)
#include "base/strings/string_util_win.h"
#elif defined(OS_POSIX)
#include "base/strings/string_util_posix.h"
#else
#error Define string operations appropriately for your platform
#endif

#endif  // BASE_STRINGS_STRING_UTIL_H_
