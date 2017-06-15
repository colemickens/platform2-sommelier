// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/sys_byteorder.h>

#include "u2fd/tpm_vendor_cmd.h"

namespace {

// All TPM extension commands use this struct for input and output. Any other
// data follows immediately after. All values are big-endian over the wire.
struct TpmCmdHeader {
  uint16_t tag;              // TPM_ST_NO_SESSIONS
  uint32_t size;             // including this header
  uint32_t code;             // Command out, Response back.
  uint16_t subcommand_code;  // Additional command/response codes
} __attribute__((packed));

// TPMv2 Spec mandates that vendor-specific command codes have bit 29 set,
// while bits 15-0 indicate the command. All other bits should be zero. We
// define one of those 16-bit command values for Cr50 purposes, and use the
// subcommand_code in struct TpmCmdHeader to further distinguish the desired
// operation.
const uint32_t kTpmCcVendorBit = 0x20000000;

// Vendor-specific command codes
const uint32_t kTpmCcVendorCr50 = 0x0000;

// Cr50 vendor-specific subcommand codes. 16 bits available.
const uint16_t kVendorCcU2fApdu = 27;

}  // namespace

namespace u2f {

TpmVendorCommandProxy::TpmVendorCommandProxy() {}
TpmVendorCommandProxy::~TpmVendorCommandProxy() {}

uint32_t TpmVendorCommandProxy::VendorCommand(uint16_t cc,
                                              const std::string& input,
                                              std::string* output) {
  // Pack up the header and the input
  TpmCmdHeader header;
  header.tag = base::HostToNet16(trunks::TPM_ST_NO_SESSIONS);
  header.size = base::HostToNet32(sizeof(header) + input.size());
  header.code = base::HostToNet32(kTpmCcVendorBit | kTpmCcVendorCr50);
  header.subcommand_code = base::HostToNet16(cc);

  std::string command(reinterpret_cast<char*>(&header), sizeof(header));
  command += input;

  // Send the command, get the response
  VLOG(2) << "Out(" << command.size()
          << "): " << base::HexEncode(command.data(), command.size());
  std::string response = SendCommandAndWait(command);
  VLOG(2) << "In(" << response.size()
          << "):  " << base::HexEncode(response.data(), response.size());

  if (response.size() < sizeof(header)) {
    LOG(ERROR) << "TPM response was too short!";
    return -1;
  }

  // Unpack the response header and any output
  memcpy(&header, response.data(), sizeof(header));
  header.size = base::NetToHost32(header.size);
  header.code = base::NetToHost32(header.code);

  // Error of some sort?
  if (header.code) {
    if ((header.code & kVendorRcErr) == kVendorRcErr) {
      LOG(WARNING) << "TPM error code 0x" << std::hex << header.code;
    }
  }

  // Pass back any reply beyond the header
  *output = response.substr(sizeof(header));

  return header.code;
}

int TpmVendorCommandProxy::SendU2fApdu(const std::string& req,
                                       std::string* resp_out) {
  return VendorCommand(kVendorCcU2fApdu, req, resp_out);
}

int TpmVendorCommandProxy::GetU2fVersion(std::string* version_out) {
  std::string ping(8, 0);
  std::string ver;

  // build the command U2F_VERSION:
  // CLA INS P1  P2  Le
  // 00  03  00  00  00
  ping[1] = 0x03;
  int rc = SendU2fApdu(ping, &ver);

  if (!rc) {
    // remove the 16-bit status code at the end
    *version_out = ver.substr(0, ver.length() - sizeof(uint16_t));
    VLOG(1) << "version " << *version_out;
  }

  return rc;
}

int TpmVendorCommandProxy::SetU2fVendorMode(uint8_t mode) {
  std::string vendor_mode(5, 0);
  std::string rmode;
  const uint8_t kCmdU2fVendorMode = 0xbf;
  const uint8_t kP1SetMode = 0x1;

  // build the command U2F_VENDOR_MODE:
  // CLA INS P1  P2  Le
  // 00  bf  01  md  00
  vendor_mode[1] = kCmdU2fVendorMode;
  vendor_mode[2] = kP1SetMode;
  vendor_mode[3] = mode;
  int rc = SendU2fApdu(vendor_mode, &rmode);

  if (!rc) {
    // remove the 16-bit status code at the end
    VLOG(1) << "current mode " << static_cast<int>(rmode[0]);
  }

  return rc;
}

}  // namespace u2f
