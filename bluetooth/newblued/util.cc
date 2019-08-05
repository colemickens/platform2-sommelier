// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bluetooth/newblued/util.h"

#include <algorithm>
#include <cstdint>
#include <regex>  // NOLINT(build/c++11)
#include <string>
#include <utility>

#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <newblue/bt.h>
#include <newblue/uuid.h>

#include "bluetooth/newblued/uuid.h"

namespace {

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

////////////////////////////////////////////////////////////////////////////////
// Miscellaneous utility functions
////////////////////////////////////////////////////////////////////////////////

uint16_t GetNumFromLE16(const uint8_t* buf) {
  return static_cast<uint16_t>(GetNumFromLE(buf, 16));
}

uint32_t GetNumFromLE24(const uint8_t* buf) {
  return static_cast<uint32_t>(GetNumFromLE(buf, 24));
}

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

////////////////////////////////////////////////////////////////////////////////
// Translation between D-Bus object path and newblued types.
////////////////////////////////////////////////////////////////////////////////

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
  std::regex rgx("/desc[0-9a-fA-F]{4}$");
  std::smatch m;
  std::string desc;

  if (!std::regex_search(*descriptor, m, rgx) || m.empty())
    return kInvalidDescriptorHandle;

  desc = m.str(0).substr(5);
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
  std::string d = base::StringPrintf("/desc%04X", desc_handle);
  if (c.empty() || d.empty())
    return "";
  return c + d;
}

////////////////////////////////////////////////////////////////////////////////
// Translation between libnewblue types and newblued types.
////////////////////////////////////////////////////////////////////////////////

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

Uuid ConvertToUuid(const struct uuid& uuid) {
  std::vector<uint8_t> uuid_value;
  uint64_t lo = uuid.lo;
  uint64_t hi = uuid.hi;
  int i;

  for (i = 0; i < sizeof(lo); ++i, lo >>= 8)
    uuid_value.emplace(uuid_value.begin(), static_cast<uint8_t>(lo));
  for (i = 0; i < sizeof(hi); ++i, hi >>= 8)
    uuid_value.emplace(uuid_value.begin(), static_cast<uint8_t>(hi));

  return Uuid(uuid_value);
}

struct uuid ConvertToRawUuid(const Uuid& uuid) {
  struct uuid u;
  uuidZero(&u);

  if (uuid.format() == UuidFormat::UUID_INVALID)
    return u;

  uint64_t tmp;
  memcpy(&tmp, &uuid.value()[0], sizeof(uint64_t));
  u.hi = be64toh(tmp);
  memcpy(&tmp, &uuid.value()[8], sizeof(uint64_t));
  u.lo = be64toh(tmp);

  return u;
}

std::unique_ptr<GattService> ConvertToGattService(
    const struct GattTraversedService& service) {
  // struct GattTraversedService is the result of primary service traversal, so
  // it is safe to assume that primary is always true in this case.
  auto s = std::make_unique<GattService>(service.firstHandle,
                                         service.lastHandle, true /* primary */,
                                         ConvertToUuid(service.uuid));

  const auto* included_service = service.inclSvcs;
  for (int i = 0; i < service.numInclSvcs; ++i, ++included_service) {
    auto is = std::make_unique<GattIncludedService>(
        s.get(), included_service->includeDefHandle,
        included_service->firstHandle, included_service->lastHandle,
        ConvertToUuid(included_service->uuid));
    s->AddIncludedService(std::move(is));
  }

  const auto* characteristic = service.chars;
  for (int i = 0; i < service.numChars; ++i, ++characteristic) {
    auto c = std::make_unique<GattCharacteristic>(
        s.get(), characteristic->valHandle, characteristic->firstHandle,
        characteristic->lastHandle, characteristic->charProps,
        ConvertToUuid(characteristic->uuid));

    const auto* descriptor = characteristic->descrs;
    for (int j = 0; j < characteristic->numDescrs; ++j, ++descriptor) {
      auto d = std::make_unique<GattDescriptor>(
          c.get(), descriptor->handle, ConvertToUuid(descriptor->uuid));
      c->AddDescriptor(std::move(d));
    }
    s->AddCharacteristic(std::move(c));
  }

  return s;
}

}  // namespace bluetooth
