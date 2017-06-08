// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/helpers/drm_display_info_reader.h"

#include <string>
#include <utility>

#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/memory/ptr_util.h>
#include <base/strings/string_util.h>
#include <base/sys_byteorder.h>

namespace debugd {

namespace {

// Look for video card subdirectories under the top-level sysfs directory.
const char kSysfsVideoCardPattern[] = "card?";

// Name of DRM sysfs file that contains whether the connector is connected to a
// display.
const char kConnectorStatusFilename[] = "status";

// Name of DRM sysfs file that contains the EDID blob from a connected display.
const char kConnectorEDIDFilename[] = "edid";

// Contains display info for a single connector.
struct ConnectorInfo {
  ConnectorInfo() : is_connected(false), model(0) {}

  // Whether the connector is connected to a display.
  bool is_connected;

  // If the display has EDID info, these are extracted from the EDID info.
  std::string manufacturer;
  uint32_t model;

  std::unique_ptr<base::DictionaryValue> ToDictionary() const {
    auto result = base::MakeUnique<base::DictionaryValue>();
    result->SetBoolean("is_connected", is_connected);
    if (!manufacturer.empty()) {
      result->SetString("manufacturer", manufacturer);
      result->SetInteger("model", model);
    }
    return result;
  }
};

// EDID info header format based on:
// https://en.wikipedia.org/wiki/Extended_Display_Identification_Data#EDID_1.3_data_format
struct EDIDHeader {
  // Fixed header pattern: 00 FF FF FF FF FF FF 00
  uint64_t header_pattern;

  // The next 4 bytes are separated into two 2-byte words for manufacturer and
  // model.
  uint16_t manufacturer;
  uint16_t model;
};

// Given a raw 16-bit manufacturer field from EDID info, returns a string
// containing the three-letter code it represents.
std::string GetManufacturerString(uint16_t manufacturer_id) {
  // This is a big-endian field that needs to be swapped on little-endian
  // machines to work properly.
  manufacturer_id = base::NetToHost16(manufacturer_id);

  // EDID info manufacturer ID code uses the following scheme to encode
  // letters as integers:
  // 1='A', 2='B', ... 26='Z'.
  //
  // The three letters are represented by 5-bit value fields:
  // Bits 10-14: first letter
  // Bits  5- 9: second letter
  // Bits  0- 4: third letter
  uint16_t letter_2 = manufacturer_id & 0x1f;
  uint16_t letter_1 = (manufacturer_id >> 5) & 0x1f;
  uint16_t letter_0 = (manufacturer_id >> 10) & 0x1f;

  std::string result;
  result.push_back('A' + letter_0 - 1);
  result.push_back('A' + letter_1 - 1);
  result.push_back('A' + letter_2 - 1);

  return result;
}

// Parses an EDID info blob to get the manufacturer and model codes.
void GetEDIDInfoFromBlob(const std::string& edid_blob,
                         std::string* manufacturer,
                         uint32_t* model) {
  if (edid_blob.size() < sizeof(EDIDHeader))
    return;

  const EDIDHeader& header =
      *reinterpret_cast<const EDIDHeader*>(edid_blob.data());
  *manufacturer = GetManufacturerString(header.manufacturer);
  // The model number field is little-endian.
  *model = base::ByteSwapToLE16(header.model);
}

// For a single connector indicated by sysfs entry |path|, returns a
// ConnectorInfo struct with its status info.
ConnectorInfo GetConnectorInfo(const base::FilePath& path) {
  ConnectorInfo result;

  std::string status;
  if (base::ReadFileToString(path.Append(kConnectorStatusFilename), &status)) {
    base::TrimWhitespaceASCII(status, base::TRIM_TRAILING, &status);
    // |status| is either "connected" or "disconnected".
    result.is_connected = (status == "connected");
  }

  std::string edid_blob;
  if (base::ReadFileToString(path.Append(kConnectorEDIDFilename), &edid_blob)) {
    GetEDIDInfoFromBlob(edid_blob, &result.manufacturer, &result.model);
  }

  return result;
}

// Scans for display info from video card sysfs path specified in |path|.
// Returns a dictionary, each entry having:
// - key: connector name.
// - value: a dictionary containing info about the connector.
std::unique_ptr<base::DictionaryValue> GetDisplayInfoForCard(
    const base::FilePath& path) {
  // e.g. under card0/, connectors are under dirs e.g. card0-HDMI-A-1/,
  // card0-eDP-1/, etc.
  std::string connector_pattern = path.BaseName().value() + "-*";

  auto result = base::MakeUnique<base::DictionaryValue>();

  base::FileEnumerator enumerator(path,
                                  false /* recursive */,
                                  base::FileEnumerator::DIRECTORIES,
                                  connector_pattern);
  for (base::FilePath connector_dir_path = enumerator.Next();
       !connector_dir_path.empty();
       connector_dir_path = enumerator.Next()) {
    // Get the part of |connector_dir_path| that comes after e.g. "card0-"
    std::string prefix = path.BaseName().value();
    std::string name = connector_dir_path.BaseName().value();
    base::ReplaceFirstSubstringAfterOffset(&name, 0, prefix + "-", "");

    // Store the connector info in the result, using the connector name as the
    // key.
    std::unique_ptr<base::DictionaryValue> info =
        GetConnectorInfo(connector_dir_path).ToDictionary();
    // TODO(sque): Use SetDictionary() when it has been incorporated into CrOS
    // on the next libchrome uprev.
    result->Set(name, std::move(info));
  }

  return result;
}

}  // namespace

std::unique_ptr<base::DictionaryValue> DRMDisplayInfoReader::GetDisplayInfo(
    const base::FilePath& path) const {
  auto result = base::MakeUnique<base::DictionaryValue>();

  base::FileEnumerator enumerator(path,
                                  false /* recursive */,
                                  base::FileEnumerator::DIRECTORIES,
                                  debugd::kSysfsVideoCardPattern);
  for (base::FilePath card_dir_path = enumerator.Next(); !card_dir_path.empty();
       card_dir_path = enumerator.Next()) {
    std::string card_name = card_dir_path.BaseName().value();

    std::unique_ptr<base::DictionaryValue> card_info =
        debugd::GetDisplayInfoForCard(card_dir_path);
    // TODO(sque): Use SetDictionary() when it has been incorporated into CrOS
    // on the next libchrome uprev.
    result->Set(card_name, std::move(card_info));
  }

  return result;
}

}  // namespace debugd
