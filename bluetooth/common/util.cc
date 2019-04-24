// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/common/util.h"

#include <newblue/bt.h>

#include <algorithm>
#include <regex>  // NOLINT(build/c++11)

#include <base/files/file_util.h>
#include <base/stl_util.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/string_number_conversions.h>

namespace {

constexpr char kNewblueConfigFile[] = "/var/lib/bluetooth/newblue";

uint64_t GetNumFromLE(const uint8_t* buf, uint8_t bits) {
  uint64_t val = 0;
  uint8_t bytes = bits / 8;

  CHECK(buf);

  buf += bytes;

  while (bytes--)
    val = (val << 8) | *--buf;

  return val;
}

}  // namespace

namespace bluetooth {

// True if the kernel is configured to split LE traffic.
bool IsBleSplitterEnabled() {
  std::string content;
  // LE splitter is enabled iff /var/lib/bluetooth/newblue starts with "1".
  if (base::ReadFileToString(base::FilePath(kNewblueConfigFile), &content)) {
    base::TrimWhitespaceASCII(content, base::TRIM_TRAILING, &content);
    if (content == "1")
      return true;
  }

  // Current LE splitter default = disabled.
  return false;
}

// Turns the content of |buf| into a uint16_t in host order. This should be used
// when reading the little-endian data from Bluetooth packet.
uint16_t GetNumFromLE16(const uint8_t* buf) {
  return static_cast<uint16_t>(GetNumFromLE(buf, 16));
}
// Turns the content of |buf| into a uint32_t in host order. This should be used
// when reading the little-endian data from Bluetooth packet.
uint32_t GetNumFromLE24(const uint8_t* buf) {
  return static_cast<uint32_t>(GetNumFromLE(buf, 24));
}

// Reverses the content of |buf| and returns bytes in big-endian order. This
// should be used when reading the little-endian data from Bluetooth packet.
std::vector<uint8_t> GetBytesFromLE(const uint8_t* buf, size_t buf_len) {
  std::vector<uint8_t> ret;

  CHECK(buf);

  if (!buf_len)
    return ret;

  ret.assign(buf, buf + buf_len);
  std::reverse(ret.begin(), ret.end());
  return ret;
}

UniqueId GetNextId() {
  static UniqueId next_id = 1;
  uint64_t id = next_id++;
  if (id)
    return id;
  next_id -= 1;
  LOG(ERROR) << "Run out of unique IDs";
  return 0;
}

bool ConvertToBtAddr(bool is_random_address,
                     const std::string& address,
                     struct bt_addr* result) {
  CHECK(result);

  std::vector<std::string> tokens = base::SplitString(
      address, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (tokens.size() != BT_MAC_LEN)
    return false;

  uint8_t addr[BT_MAC_LEN];
  uint8_t* ptr = addr + BT_MAC_LEN;
  for (const auto& token : tokens) {
    uint32_t value;
    if (token.size() != 2 || !base::HexStringToUInt(token, &value))
      return false;
    *(--ptr) = static_cast<uint8_t>(value);
  }

  memcpy(result->addr, addr, BT_MAC_LEN);
  result->type =
      is_random_address ? BT_ADDR_TYPE_LE_RANDOM : BT_ADDR_TYPE_LE_PUBLIC;
  return true;
}

bool TrimAdapterFromObjectPath(std::string* path) {
  std::regex rgx("^/org/bluez/hci[0-9]+$");
  std::smatch match;

  if (!std::regex_search(*path, match, rgx) || match.size() != 1)
    return false;

  path->clear();
  return true;
}

std::string TrimDeviceFromObjectPath(std::string* device) {
  std::regex rgx("/dev_([0-9a-fA-F]{2}_){5}[0-9a-fA-F]{2}$");
  std::smatch m;
  std::string address;

  if (!std::regex_search(*device, m, rgx) || m.empty())
    return "";

  address = m.str(0).substr(5);
  std::replace(address.begin(), address.end(), '_', ':');
  *device = device->substr(0, device->size() - m.str(0).size());
  return address;
}

int32_t TrimServiceFromObjectPath(std::string* service) {
  std::regex rgx("/service[0-9a-fA-F]{4}$");
  std::smatch m;
  std::string srv;

  if (!std::regex_search(*service, m, rgx) || m.empty())
    return kInvalidServiceHandle;

  srv = m.str(0).substr(8, 4);
  *service = service->substr(0, service->size() - m.str(0).size());
  return std::stol(srv, nullptr, 16);
}

int32_t TrimCharacteristicFromObjectPath(std::string* characteristic) {
  std::regex rgx("/char[0-9a-fA-F]{4}$");
  std::smatch m;
  std::string charac;

  if (!std::regex_search(*characteristic, m, rgx) || m.empty())
    return kInvalidCharacteristicHandle;

  charac = m.str(0).substr(5, 4);
  *characteristic =
      characteristic->substr(0, characteristic->size() - m.str(0).size());
  return std::stol(charac, nullptr, 16);
}

int32_t TrimDescriptorFromObjectPath(std::string* descriptor) {
  std::regex rgx("/descriptor[0-9a-fA-F]{4}$");
  std::smatch m;
  std::string desc;

  if (!std::regex_search(*descriptor, m, rgx) || m.empty())
    return kInvalidDescriptorHandle;

  desc = m.str(0).substr(11);
  *descriptor = descriptor->substr(0, descriptor->size() - m.str(0).size());
  return std::stol(desc, nullptr, 16);
}

std::string ConvertDeviceObjectPathToAddress(const std::string& path) {
  std::string p(path);
  std::string address = TrimDeviceFromObjectPath(&p);

  if (address.empty() || p.empty())
    return "";

  if (!TrimAdapterFromObjectPath(&p))
    return "";

  return address;
}

std::string ConvertDeviceAddressToObjectPath(const std::string& address) {
  std::string path;

  if (address.empty())
    return "";

  path = base::StringPrintf("%s/dev_%s", kAdapterObjectPath, address.c_str());
  std::replace(path.begin(), path.end(), ':', '_');
  return path;
}

bool ConvertServiceObjectPathToHandle(std::string* address,
                                      uint16_t* handle,
                                      const std::string& path) {
  std::string p(path);
  std::string addr;
  int32_t h = TrimServiceFromObjectPath(&p);
  if (h == kInvalidServiceHandle || p.empty())
    return false;

  addr = ConvertDeviceObjectPathToAddress(p);
  if (addr.empty())
    return false;

  *address = addr;
  *handle = h;
  return true;
}

std::string ConvertServiceHandleToObjectPath(const std::string& address,
                                             uint16_t handle) {
  std::string dev = ConvertDeviceAddressToObjectPath(address);
  std::string s = base::StringPrintf("/service%04X", handle);
  if (dev.empty() || s.empty())
    return "";
  return dev + s;
}

bool ConvertCharacteristicObjectPathToHandles(std::string* address,
                                              uint16_t* service_handle,
                                              uint16_t* char_handle,
                                              const std::string& path) {
  std::string p(path);
  std::string addr;
  uint16_t sh;
  int32_t ch = TrimCharacteristicFromObjectPath(&p);
  if (ch == kInvalidCharacteristicHandle || p.empty())
    return false;

  if (!ConvertServiceObjectPathToHandle(&addr, &sh, p))
    return false;

  *address = addr;
  *service_handle = sh;
  *char_handle = ch;
  return true;
}

std::string ConvertCharacteristicHandleToObjectPath(const std::string& address,
                                                    uint16_t service_handle,
                                                    uint16_t char_handle) {
  std::string s = ConvertServiceHandleToObjectPath(address, service_handle);
  std::string c = base::StringPrintf("/char%04X", char_handle);
  if (s.empty() || c.empty())
    return "";
  return s + c;
}

bool ConvertDescriptorObjectPathToHandles(std::string* address,
                                          uint16_t* service_handle,
                                          uint16_t* char_handle,
                                          uint16_t* desc_handle,
                                          const std::string& path) {
  std::string p(path);
  std::string addr;
  uint16_t sh;
  uint16_t ch;
  int32_t dh = TrimDescriptorFromObjectPath(&p);
  if (dh == kInvalidDescriptorHandle || p.empty())
    return false;

  if (!ConvertCharacteristicObjectPathToHandles(&addr, &sh, &ch, p))
    return false;

  *address = addr;
  *service_handle = sh;
  *char_handle = ch;
  *desc_handle = dh;
  return true;
}

std::string ConvertDescriptorHandleToObjectPath(const std::string& address,
                                                uint16_t service_handle,
                                                uint16_t char_handle,
                                                uint16_t desc_handle) {
  std::string c = ConvertCharacteristicHandleToObjectPath(
      address, service_handle, char_handle);
  std::string d = base::StringPrintf("/descriptor%04X", desc_handle);
  if (c.empty() || d.empty())
    return "";
  return c + d;
}

void OnInterfaceExported(std::string object_path,
                         std::string interface_name,
                         bool success) {
  VLOG(1) << "Completed interface export " << interface_name << " of object "
          << object_path << ", success = " << success;
}

}  // namespace bluetooth
