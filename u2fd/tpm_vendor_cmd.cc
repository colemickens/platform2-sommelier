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

void TpmVendorCommandProxy::LogIndividualCertificate() {
  //  ISO7816-4:2005 APDU header size.
  constexpr uint8_t kApduHeaderSize = 5;
  // U2F_REGISTER command instruction index.
  constexpr uint8_t kCmdU2fRegister = 0x01;
  // U2F_REGISTER flag in the APDU P1 field.
  constexpr uint8_t kG2fAttest = 0x80;
  // U2F_REGISTER payload is the nonce and appid (usually 2x SHA-256 = 64B).
  constexpr uint8_t kRegisterPayloadSize = 2 * 32;
  // ECDSA P256 uses 256-bit integers.
  constexpr size_t kP256NBytes = 256 / 8;
  // ASN.1 DER constants we are using for certificate parsing.
  constexpr uint8_t kAsn1ClassStructured = 0x20;
  constexpr uint8_t kAsn1TagSequence = 0x10;
  constexpr uint8_t kAsn1LengthLong = 0x80;
  // Size of the Type and Length prefix for a Sequence with a 2-byte size.
  constexpr size_t kAsn1SequenceTagSize = 2 + sizeof(uint16_t);

  // build the U2F_REGISTER command:
  // CLA INS P1  P2  Lc  Payload
  // 00  01  80  00  00  (dummy) nonce and appid
  std::string cmd(kApduHeaderSize + kRegisterPayloadSize, 0);
  cmd[1] = kCmdU2fRegister;
  cmd[2] = kG2fAttest;
  cmd[4] = kRegisterPayloadSize;

  std::string resp;
  if (SendU2fApdu(cmd, &resp) != 0) {
    VLOG(1) << "Cannot retrieve individual attestation certificate";
    return;
  }

  // The response is:
  // A reserved byte [1 byte], which for legacy reasons has the value 0x05.
  // A user public key [65 bytes]. This is the (uncompressed) x,y-representation
  //                               of a curve point on the P-256 elliptic curve.
  // A key handle length byte [1 byte], which specifies the length of the key
  //                                    handle (see below).
  //                                    The value is unsigned (range 0-255).
  // A key handle [length, see previous field]. This a handle that
  //                                    allows the U2F token to identify the
  //                                    generated key pair. U2F tokens may wrap
  //                                    the generated private key and the
  //                                    application id it was generated for,
  //                                    and output that as the key handle.
  // An attestation certificate [variable length]. This is a certificate in
  //                                    X.509 DER format.
  // A signature. This is a ECDSA signature (on P-256).
  const int pkey_offset = 1;
  const size_t pkey_size = 1 + 2 * kP256NBytes;
  const int handle_len_offset = pkey_offset + pkey_size;
  const int handle_offset = handle_len_offset + 1;

  // Validate the length up to the certificate prefix.
  if (resp.size() < handle_offset)
    return;
  uint8_t handle_len = resp[handle_len_offset];
  const int cert_offset = handle_offset + handle_len;
  if (resp.size() < cert_offset + kAsn1SequenceTagSize)
    return;

  // parse the first tag of the certificate ASN.1 data to know its length.
  std::string cert_seq_tag = resp.substr(cert_offset, kAsn1SequenceTagSize);

  // Should be a Constructed Sequence ASN.1 tag else all bets are off,
  // with the size taking 2 bytes (the certificate size is somewhere between
  // 256B and 2KB).
  if ((static_cast<uint8_t>(cert_seq_tag[0]) !=
       (kAsn1ClassStructured | kAsn1TagSequence)) ||
      (static_cast<uint8_t>(cert_seq_tag[1]) !=
       (kAsn1LengthLong | sizeof(uint16_t)))) {
    VLOG(1) << "Cannot parse certificate";
    return;
  }

  uint16_t length_tag;
  memcpy(&length_tag, cert_seq_tag.c_str() + 2, sizeof(length_tag));
  size_t cert_size = base::NetToHost16(length_tag) + cert_seq_tag.size();
  // Validate the length of the certificate.
  if (resp.size() < cert_offset + cert_size)
    return;

  VLOG(1) << "Certificate: "
          << base::HexEncode(resp.data() + cert_offset, cert_size);
}

int TpmVendorCommandProxy::SetU2fVendorMode(uint8_t mode) {
  std::string vendor_mode(5, 0);
  std::string rmode;
  const uint8_t kCmdU2fVendorMode = 0xbf;
  const uint8_t kP1SetMode = 0x1;
  const uint8_t kU2fExtended = 3;

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
    // record the individual attestation certificate if the extension is on.
    if (rmode[0] == kU2fExtended && VLOG_IS_ON(1))
      LogIndividualCertificate();
  }

  return rc;
}

}  // namespace u2f
