// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/server_backed_state_key_generator.h"

#include <stdint.h>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/stl_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <crypto/sha2.h>

#include "login_manager/system_utils.h"

namespace login_manager {

namespace {

// Characters to trim from values.
const char kTrimChars[] = "\" ";

// Keys in the tool-provided key-value pairs.
const char kGroupCodeKey[] = "gbind_attribute";
const char kDiskSerialNumberKey[] = "root_disk_serial_number";

// These are the machine serial number keys that we check in order until we
// find a non-empty serial number. The VPD spec says the serial number should be
// in the "serial_number" key for v2+ VPDs. However, legacy devices used a
// different key to report their serial number, which we fall back to if
// "serial_number" is not present.
//
// Product_S/N is still special-cased due to inconsistencies with serial
// numbers on Lumpy devices: On these devices, serial_number is identical to
// Product_S/N with an appended checksum. Unfortunately, the sticker on the
// packaging doesn't include that checksum either (the sticker on the device
// does though!). The former sticker is the source of the serial number used by
// device management service, so we prefer Product_S/N over serial number to
// match the server.
//
// TODO(mnissler): Move serial_number back to the top once the server side uses
// the correct serial number.
const char* kMachineInfoSerialNumberKeys[] = {
    "Product_S/N",    // Lumpy/Alex devices
    "serial_number",  // VPD v2+ devices
    "Product_SN",     // Mario
    "sn",             // old ZGB devices (more recent ones use serial_number)
};

std::string GetMapValue(const std::map<std::string, std::string>& map,
                        const std::string& key) {
  std::map<std::string, std::string>::const_iterator entry = map.find(key);
  return entry == map.end() ? std::string() : entry->second;
}

}  // namespace

const int ServerBackedStateKeyGenerator::kDeviceStateKeyTimeQuantumPower;
const int ServerBackedStateKeyGenerator::kDeviceStateKeyFutureQuanta;

ServerBackedStateKeyGenerator::ServerBackedStateKeyGenerator(
    SystemUtils* system_utils)
    : system_utils_(system_utils), machine_info_available_(false) {
}

ServerBackedStateKeyGenerator::~ServerBackedStateKeyGenerator() {
}

// static
bool ServerBackedStateKeyGenerator::ParseMachineInfo(
    const std::string& data,
    std::map<std::string, std::string>* params) {
  params->clear();

  // Parse the name-value pairs list. The return value of
  // SplitStringIntoKeyValuePairs is deliberately ignored in order to handle
  // comment lines (those start with a #) emitted by dump_vpd_log.
  base::StringPairs pairs;
  base::SplitStringIntoKeyValuePairs(data, '=', '\n', &pairs);

  for (base::StringPairs::const_iterator pair(pairs.begin());
       pair != pairs.end();
       ++pair) {
    std::string name;
    base::TrimString(pair->first, kTrimChars, &name);
    if (name.empty())
      continue;

    std::string value;
    base::TrimString(pair->second, kTrimChars, &value);
    (*params)[name] = value;
  }

  return !params->empty();
}

bool ServerBackedStateKeyGenerator::InitMachineInfo(
    const std::map<std::string, std::string>& params) {
  machine_info_available_ = true;

  for (size_t i = 0; i < arraysize(kMachineInfoSerialNumberKeys); i++) {
    std::string candidate =
        GetMapValue(params, kMachineInfoSerialNumberKeys[i]);
    if (!candidate.empty()) {
      machine_serial_number_ = candidate;
      break;
    }
  }
  group_code_key_ = GetMapValue(params, kGroupCodeKey);
  disk_serial_number_ = GetMapValue(params, kDiskSerialNumberKey);

  LOG_IF(ERROR, machine_serial_number_.empty())
      << "Machine serial number missing!";
  LOG_IF(ERROR, disk_serial_number_.empty()) << "Disk serial number missing!";

  // Fire all pending callbacks.
  std::vector<std::vector<uint8_t> > state_keys;
  ComputeKeys(&state_keys);
  std::vector<StateKeyCallback> callbacks;
  callbacks.swap(pending_callbacks_);
  for (
      std::vector<StateKeyCallback>::const_iterator callback(callbacks.begin());
      callback != callbacks.end();
      ++callback) {
    callback->Run(state_keys);
  }

  return !machine_serial_number_.empty() && !disk_serial_number_.empty();
}

void ServerBackedStateKeyGenerator::RequestStateKeys(
    const StateKeyCallback& callback) {
  if (!machine_info_available_) {
    pending_callbacks_.push_back(callback);
    return;
  }

  std::vector<std::vector<uint8_t> > state_keys;
  ComputeKeys(&state_keys);
  callback.Run(state_keys);
}

void ServerBackedStateKeyGenerator::ComputeKeys(
    std::vector<std::vector<uint8_t> >* state_keys) {
  state_keys->clear();

  if (machine_serial_number_.empty() || disk_serial_number_.empty())
    return;

  // Get the current time in quantized form.
  int64_t quantum_size = 1 << kDeviceStateKeyTimeQuantumPower;
  int64_t quantized_time = system_utils_->time(NULL) & ~(quantum_size - 1);

  // Compute the state keys.
  for (int i = 0; i < kDeviceStateKeyFutureQuanta; ++i) {
    state_keys->push_back(std::vector<uint8_t>(crypto::kSHA256Length));
    crypto::SHA256HashString(
        crypto::SHA256HashString(group_code_key_) +
            crypto::SHA256HashString(disk_serial_number_) +
            crypto::SHA256HashString(machine_serial_number_) +
            crypto::SHA256HashString(base::Int64ToString(quantized_time)),
        vector_as_array(&state_keys->back()),
        state_keys->back().size());
    quantized_time += quantum_size;
  }
}

}  // namespace login_manager
