// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"

#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"

namespace {

typedef uintptr_t MachineWord;
const uintptr_t kMachineWordAlignmentMask = sizeof(MachineWord) - 1;

inline bool IsAlignedToMachineWord(const void* pointer) {
  return !(reinterpret_cast<MachineWord>(pointer) & kMachineWordAlignmentMask);
}

template<typename T> inline T* AlignToMachineWord(T* pointer) {
  return reinterpret_cast<T*>(reinterpret_cast<MachineWord>(pointer) &
                              ~kMachineWordAlignmentMask);
}

template<size_t size, typename CharacterType> struct NonASCIIMask;
template<> struct NonASCIIMask<4, char> {
    static inline uint32_t value() { return 0x80808080U; }
};
template<> struct NonASCIIMask<8, char> {
    static inline uint64_t value() { return 0x8080808080808080ULL; }
};

}  // namespace

namespace base {

template<typename STR>
bool ReplaceCharsT(const STR& input,
                   const STR& replace_chars,
                   const STR& replace_with,
                   STR* output) {
  bool removed = false;
  size_t replace_length = replace_with.length();

  *output = input;

  size_t found = output->find_first_of(replace_chars);
  while (found != STR::npos) {
    removed = true;
    output->replace(found, 1, replace_with);
    found = output->find_first_of(replace_chars, found + replace_length);
  }

  return removed;
}

bool ReplaceChars(const std::string& input,
                  const base::StringPiece& replace_chars,
                  const std::string& replace_with,
                  std::string* output) {
  return ReplaceCharsT(input, replace_chars.as_string(), replace_with, output);
}

template<typename Str>
TrimPositions TrimStringT(const Str& input,
                          BasicStringPiece<Str> trim_chars,
                          TrimPositions positions,
                          Str* output) {
  // Find the edges of leading/trailing whitespace as desired. Need to use
  // a StringPiece version of input to be able to call find* on it with the
  // StringPiece version of trim_chars (normally the trim_chars will be a
  // constant so avoid making a copy).
  BasicStringPiece<Str> input_piece(input);
  const size_t last_char = input.length() - 1;
  const size_t first_good_char = (positions & TRIM_LEADING) ?
      input_piece.find_first_not_of(trim_chars) : 0;
  const size_t last_good_char = (positions & TRIM_TRAILING) ?
      input_piece.find_last_not_of(trim_chars) : last_char;

  // When the string was all trimmed, report that we stripped off characters
  // from whichever position the caller was interested in. For empty input, we
  // stripped no characters, but we still need to clear |output|.
  if (input.empty() ||
      (first_good_char == Str::npos) || (last_good_char == Str::npos)) {
    bool input_was_empty = input.empty();  // in case output == &input
    output->clear();
    return input_was_empty ? TRIM_NONE : positions;
  }

  // Trim.
  *output =
      input.substr(first_good_char, last_good_char - first_good_char + 1);

  // Return where we trimmed from.
  return static_cast<TrimPositions>(
      ((first_good_char == 0) ? TRIM_NONE : TRIM_LEADING) |
      ((last_good_char == last_char) ? TRIM_NONE : TRIM_TRAILING));
}

bool TrimString(const std::string& input,
                base::StringPiece trim_chars,
                std::string* output) {
  return TrimStringT(input, trim_chars, TRIM_ALL, output) != TRIM_NONE;
}

template<typename Str>
BasicStringPiece<Str> TrimStringPieceT(BasicStringPiece<Str> input,
                                       BasicStringPiece<Str> trim_chars,
                                       TrimPositions positions) {
  size_t begin = (positions & TRIM_LEADING) ?
      input.find_first_not_of(trim_chars) : 0;
  size_t end = (positions & TRIM_TRAILING) ?
      input.find_last_not_of(trim_chars) + 1 : input.size();
  return input.substr(begin, end - begin);
}

StringPiece TrimString(StringPiece input,
                       const base::StringPiece& trim_chars,
                       TrimPositions positions) {
  return TrimStringPieceT(input, trim_chars, positions);
}

TrimPositions TrimWhitespaceASCII(const std::string& input,
                                  TrimPositions positions,
                                  std::string* output) {
  return TrimStringT(input, StringPiece(kWhitespaceASCII), positions, output);
}

template <class Char>
inline bool DoIsStringASCII(const Char* characters, size_t length) {
  MachineWord all_char_bits = 0;
  const Char* end = characters + length;

  // Prologue: align the input.
  while (!IsAlignedToMachineWord(characters) && characters != end) {
    all_char_bits |= *characters;
    ++characters;
  }

  // Compare the values of CPU word size.
  const Char* word_end = AlignToMachineWord(end);
  const size_t loop_increment = sizeof(MachineWord) / sizeof(Char);
  while (characters < word_end) {
    all_char_bits |= *(reinterpret_cast<const MachineWord*>(characters));
    characters += loop_increment;
  }

  // Process the remaining bytes.
  while (characters != end) {
    all_char_bits |= *characters;
    ++characters;
  }

  MachineWord non_ascii_bit_mask =
      NonASCIIMask<sizeof(MachineWord), Char>::value();
  return !(all_char_bits & non_ascii_bit_mask);
}

bool IsStringASCII(const StringPiece& str) {
  return DoIsStringASCII(str.data(), str.length());
}

bool IsStringUTF8(const StringPiece& str) {
  const char *src = str.data();
  int32 src_len = static_cast<int32>(str.length());
  int32 char_index = 0;

  while (char_index < src_len) {
    int32 code_point;
    CBU8_NEXT(src, char_index, src_len, code_point);
    if (!IsValidCharacter(code_point))
      return false;
  }
  return true;
}

}  // namespace base
