// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Plugin tests link against this file, but not against the rest of
// cromo.  Therefore this file should not have dependencies on the
// rest of cromo.
#include "utilities.h"

#include <glog/logging.h>
#include <stdio.h>
#include <stdlib.h>

#include <iomanip>
#include <sstream>

namespace utilities {

const char* ExtractString(const DBusPropertyMap properties,
                          const char* key,
                          const char* not_found_response,
                          DBus::Error& error) {
  DBusPropertyMap::const_iterator p;
  const char* to_return = not_found_response;
  try {
    p = properties.find(key);
    if (p != properties.end()) {
      to_return = p->second.reader().get_string();
    }
  } catch (const DBus::Error& e) {
    LOG(ERROR)<<"Bad type for: " << key;
    // Setting an already-set error causes an assert fail inside dbus-c++.
    if (!error.is_set()) {
    // NB: the copy constructor for DBus::Error causes a template to
    // be instantiated that kills our -Werror build
      error.set(e.name(), e.message());
    }
  }
  return to_return;
}

uint32_t ExtractUint32(const DBusPropertyMap properties,
                       const char* key,
                       uint32_t not_found_response,
                       DBus::Error& error) {
  DBusPropertyMap::const_iterator p;
  unsigned int to_return = not_found_response;
  try {
    p = properties.find(key);
    if (p != properties.end()) {
      to_return = p->second.reader().get_uint32();
    }
  } catch (const DBus::Error& e) {
    LOG(ERROR)<<"Bad type for: " << key;
    // Setting an already-set error causes an assert fail inside dbus-c++.
    if (!error.is_set()) {
    // NB: the copy constructor for DBus::Error causes a template to
    // be instantiated that kills our -Werror build
      error.set(e.name(), e.message());
    }
  }
  return to_return;
}

bool HexEsnToDecimal(const std::string& esn_hex, std::string* out) {
  size_t length = esn_hex.length();
  if (length > 8) {
    LOG(ERROR) << "Long ESN: " << esn_hex;
    return false;
  }
  errno = 0;
  const char* start = esn_hex.c_str();
  char* end;
  uint32_t esn = strtoul(start, &end, 16);
  if (errno != 0 || *end != '\0') {
    LOG(ERROR) << "Bad ESN: " << esn_hex;
    return false;
  }
  uint32_t mfr = (esn >> 24) & 0xff;
  uint32_t serial = esn & 0x00ffffff;

  // Decimal ESN is 11 chars
  char decimal[12];
  memset(decimal, '\0', sizeof(decimal));
  int rc = snprintf(decimal, sizeof(decimal), "%03d%08d", mfr, serial);
  if (rc != 11) {
    LOG(ERROR) << "Format failure";
    return false;
  }
  if (out) {
    *out = decimal;
  }
  return true;
}

#define C(c)  {c}
#define C2(c) {0xc2, c}
#define C3(c) {0xc3, c}
#define CE(c) {0xce, c}

static uint8_t Gsm7ToUtf8Map[][3] = {
   C('@'), C2(0xa3),  C('$'),  C2(0xa5), C3(0xa8), C3(0xa9), C3(0xb9), C3(0xac),
 C3(0xb2), C3(0x87),  C('\n'), C3(0x98), C3(0xb8),  C('\r'), C3(0x85), C3(0xa5),
 CE(0x94), C('_'),   CE(0xa6), CE(0x93), CE(0x9b), CE(0xa9), CE(0xa0), CE(0xa8),
 CE(0xa3), CE(0x98), CE(0x9e),   C(' '), C3(0x86), C3(0xa6), C3(0x9f), C3(0x89),
   C(' '),   C('!'),   C('"'),   C('#'), C2(0xa4),   C('%'),   C('&'),  C('\''),
   C('('),   C(')'),   C('*'),   C('+'),   C(','),   C('-'),   C('.'),   C('/'),
   C('0'),   C('1'),   C('2'),   C('3'),   C('4'),   C('5'),   C('6'),   C('7'),
   C('8'),   C('9'),   C(':'),   C(';'),   C('<'),   C('='),   C('>'),   C('?'),
 C2(0xa1),   C('A'),   C('B'),   C('C'),   C('D'),   C('E'),   C('F'),   C('G'),
   C('H'),   C('I'),   C('J'),   C('K'),   C('L'),   C('M'),   C('N'),   C('O'),
   C('P'),   C('Q'),   C('R'),   C('S'),   C('T'),   C('U'),   C('V'),   C('W'),
   C('X'),   C('Y'),   C('Z'), C3(0x84), C3(0x96), C3(0x91), C3(0x9c), C2(0xa7),
 C2(0xbf),   C('a'),   C('b'),   C('c'),   C('d'),   C('e'),   C('f'),   C('g'),
   C('h'),   C('i'),   C('j'),   C('k'),   C('l'),   C('m'),   C('n'),   C('o'),
   C('p'),   C('q'),   C('r'),   C('s'),   C('t'),   C('u'),   C('v'),   C('w'),
   C('x'),   C('y'),   C('z'), C3(0xa4), C3(0xb6), C3(0xb1), C3(0xbc), C3(0xa0)
};

// 2nd dimension == 5 ensures that all the sub-arrays end with a 0 byte
static uint8_t ExtGsm7ToUtf8Map[][5] = {
  {0x0a, 0xc},
  {0x14, '^'},
  {0x28, '{'},
  {0x29, '}'},
  {0x2f, '\\'},
  {0x3c, '['},
  {0x3d, '~'},
  {0x3e, ']'},
  {0x40, '|'},
  {0x65, 0xe2, 0x82, 0xac}
};

static std::map<std::pair<uint8_t, uint8_t>,char> Utf8ToGsm7Map;

/*
 * Convert a packed GSM7 encoded string to UTF-8 by first unpacking
 * the string into septets, then mapping each character to its UTF-8
 * equivalent.
 */
std::string Gsm7ToUtf8String(const uint8_t* gsm7) {
  size_t datalen = *gsm7++;
  uint8_t* septets = new uint8_t[datalen];
  uint8_t saved_bits = 0;
  size_t written = 0;
  uint8_t* cp = septets;
  int i = 0;

  // unpack
  while (written < datalen) {
    for (int j = 0; written < datalen && j < 7; j++) {
      uint8_t octet = gsm7[i];
      uint8_t mask = 0xff >> (j + 1);
      uint8_t c = ((octet & mask) << j) | saved_bits;
      *cp++ = c;
      ++written;
      saved_bits = (octet & ~mask) >> (7 - j);
      ++i;
    }
    if (written < datalen) {
      *cp++ = saved_bits;
      ++written;
    }
    saved_bits = 0;
  }
  int nseptets = cp - septets;
  cp = septets;

  // now map the septets into their corresponding UTF-8 characters
  std::string str;
  for (i = 0; i < nseptets; ++i, ++cp) {
    uint8_t* mp = NULL;
    if (*cp == 0x1b) {
      ++cp;
      for (int k = 0; k < 10; k++) {
        mp = &ExtGsm7ToUtf8Map[k][0];
        if (*mp == *cp) {
          ++mp;
          ++i;
          break;
        }
      }
    } else {
      mp = &Gsm7ToUtf8Map[*cp][0];
    }
    if (mp != NULL)
      while (*mp != '\0')
        str += *mp++;
  }

  delete septets;
  return str;
}

static void InitializeUtf8ToGsm7Map() {
  for (int i = 0; i < 128; i++) {
    Utf8ToGsm7Map[std::pair<uint8_t,uint8_t>(Gsm7ToUtf8Map[i][0],
                                             Gsm7ToUtf8Map[i][1])] = i;

  }
}

std::vector<uint8_t> Utf8StringToGsm7(const std::string& input) {
  if (Utf8ToGsm7Map.size() == 0)
    InitializeUtf8ToGsm7Map();

  std::vector<uint8_t> septets;
  size_t length = input.length();
  size_t num_septets = 0;

  // First map each UTF-8 character to its GSM7 equivalent.
  for (size_t i = 0; i < length; i++) {
    std::pair<uint8_t, uint8_t> chpair;
    char ch = input.at(i);
    uint8_t thirdch = 0xff;
    // Check whether this is a one byte UTF-8 sequence, or the
    // start of a two or three byte sequence.
    if ((ch & 0x80) == 0) {
      chpair.first = ch;
      chpair.second = 0;
    } else if ((ch & 0xe0) == 0xc0) {
      chpair.first = ch;
      chpair.second = input.at(++i);
    } else if ((ch & 0xf0) == 0xe0) {
      chpair.first = ch;
      chpair.second = input.at(++i);
      thirdch = input.at(++i);
    }
    std::map<std::pair<uint8_t,uint8_t>, char>::iterator it;
    it = Utf8ToGsm7Map.find(chpair);
    if (it != Utf8ToGsm7Map.end()) {
      septets.push_back(it->second);
    } else {
      // not found in the map. search the list of extended characters,
      // but first handle one special case for the 3-byte Euro sign
      if (chpair.first == 0xe2 && chpair.second == 0x82 && thirdch == 0xac) {
        septets.push_back(0x1b);
        septets.push_back(0x65);
      } else if (chpair.second == 0) {
        for (int j = 0; j < 9; ++j) {
          if (ExtGsm7ToUtf8Map[j][1] == chpair.first) {
            septets.push_back(0x1b);
            septets.push_back(ExtGsm7ToUtf8Map[j][0]);
          }
        }
      }
    }
    // If character wasn't mapped successfully, insert a space
    if (septets.size() == num_septets)
      septets.push_back(' ');
    num_septets = septets.size();
  }

  // Now pack the septets into octets. The
  // first byte gives the number of septets.
  std::vector<uint8_t> octets;

  octets.push_back(num_septets);
  int shift = 0;
  for (size_t k = 0; k < num_septets; ++k) {
    uint8_t septet = septets.at(k);
    uint8_t octet;
    if (shift != 7) {
      octet = (septet >> shift);
      if (k < num_septets - 1)
        octet |= septets.at(k+1) << (7-shift);
      octets.push_back(octet);
    }
    if (++shift == 8)
      shift = 0;
  }

  return octets;
}

void DumpHex(const uint8_t* buf, size_t size) {
  size_t nlines = (size+15) / 16;
  size_t limit;

  for (size_t i = 0; i < nlines; i++) {
    std::ostringstream ostr;
    ostr << std::hex;
    ostr.fill('0');
    ostr.width(8);
    if (i*16 + 16 >= size)
      limit = size - i*16;
    else
      limit = 16;
    ostr << i*16 << "  ";
    ostr.fill('0');
    ostr.width(2);
    for (size_t j = 0; j < limit; j++) {
      uint8_t byte = buf[i*16+j];
      ostr << std::setw(0) << " " << std::setw(2) << std::setfill('0')
           << static_cast<unsigned int>(byte);
    }
    LOG(INFO) << ostr.str();
  }
}

}  // namespace utilities
