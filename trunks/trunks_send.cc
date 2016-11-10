//
// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdio.h>

#include <base/command_line.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/sys_byteorder.h>
#include <brillo/syslog_logging.h>

#include "trunks/trunks_dbus_proxy.h"

namespace {

using trunks::TrunksDBusProxy;

// Commands we support
constexpr char kRaw[] = "raw";
constexpr char kGetLock[] = "get_lock";
constexpr char kSetLock[] = "set_lock";
constexpr char kVerbose[] = "verbose";

static int verbose;

void PrintUsage() {
  printf("Usage:\n");
  printf("  trunks_send --%s\n", kGetLock);
  printf("  trunks_send --%s\n", kSetLock);
  printf("  trunks_send --%s XX [XX ..]\n", kRaw);
  printf("Options:\n");
  printf("   --%s\n", kVerbose);
}

std::string HexEncode(const std::string& bytes) {
  return base::HexEncode(bytes.data(), bytes.size());
}

// All TPM extension commands use this struct for input and output. Any other
// data follows immediately after. All values are big-endian over the wire.
struct TpmCmdHeader{
  uint16_t tag;                         // TPM_ST_NO_SESSIONS
  uint32_t size;                        // including this header
  uint32_t code;                        // Command out, Response back.
  uint16_t subcommand_code;             // Additional command/response codes
} __attribute__((packed));

// TPMv2 Spec mandates that vendor-specific command codes have bit 29 set,
// while bits 15-0 indicate the command. All other bits should be zero. We
// define one of those 16-bit command values for Cr50 purposes, and use the
// subcommand_code in struct TpmCmdHeader to further distinguish the desired
// operation.
#define TPM_CC_VENDOR_BIT   0x20000000
#define VENDOR_CC_MASK      0x0000ffff
// Vendor-specific command codes
#define TPM_CC_VENDOR_CR50      0x0000

// Cr50 vendor-specific subcommand codes. 16 bits available.
enum vendor_cmd_cc {
  VENDOR_CC_GET_LOCK = 16,
  VENDOR_CC_SET_LOCK = 17,
};

// The TPM response code is all zero for success.
// Errors are a little complicated:
//
//   Bits 31:12 must be zero.
//
//   Bit 11     S=0   Error
//   Bit 10     T=1   Vendor defined response code
//   Bit  9     r=0   reserved
//   Bit  8     V=1   Conforms to TPMv2 spec
//   Bit  7     F=0   Conforms to Table 14, Format-Zero Response Codes
//   Bits 6:0   num   128 possible failure reasons
#define VENDOR_RC_ERR  0x00000500
#define VENDOR_RC_MASK 0x0000007f

}  // namespace

// Send raw, unformatted bytes
static int HandleRaw(TrunksDBusProxy* proxy, base::CommandLine* cl) {
  std::string commandline;
  for (std::string arg : cl->GetArgs()) {
    commandline.append(arg);
  }
  base::RemoveChars(commandline, " \t\r\n:.", &commandline);

  std::vector<uint8_t> bytes;
  if (!base::HexStringToBytes(commandline, &bytes)) {
    LOG(ERROR) << "Can't convert input to bytes.";
    return 1;
  }

  std::string command(bytes.data(), bytes.data() + bytes.size());
  if (verbose) {
    printf("Out(%zd): ", command.size());
    puts(HexEncode(command).c_str());
  }
  std::string response = proxy->SendCommandAndWait(command);
  if (verbose) {
    printf("In(%zd):  ", response.size());
  }

  // Just print the result
  puts(HexEncode(response).c_str());

  return 0;
}

// Send the TPM command, get the reply, return response code and results.
static uint32_t VendorCommand(TrunksDBusProxy* proxy, uint16_t cc,
                              const std::string& input,
                              std::string* output) {
  // Pack up the header and the input
  struct TpmCmdHeader header;
  header.tag = base::HostToNet16(trunks::TPM_ST_NO_SESSIONS);
  header.size = base::HostToNet32(sizeof(header) + input.size());
  header.code = base::HostToNet32(TPM_CC_VENDOR_BIT | TPM_CC_VENDOR_CR50);
  header.subcommand_code = base::HostToNet16(cc);

  std::string command(reinterpret_cast<char*>(&header), sizeof(header));
  command += input;

  // Send the command, get the response
  if (verbose) {
    printf("Out(%zd): ", command.size());
    puts(HexEncode(command).c_str());
  }
  std::string response = proxy->SendCommandAndWait(command);
  if (verbose) {
    printf("In(%zd):  ", response.size());
    puts(HexEncode(response).c_str());
  }

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
    if ((header.code & VENDOR_RC_ERR) == VENDOR_RC_ERR) {
      fprintf(stderr, "TPM error code 0x%08x\n", header.code);
    }
  }

  // Pass back any reply beyond the header
  *output = response.substr(sizeof(header));

  return header.code;
}

// Vendor command to get the console lock state
static int VcGetLock(TrunksDBusProxy* proxy, base::CommandLine* cl)
{
  std::string out;
  uint32_t rc = VendorCommand(proxy, VENDOR_CC_GET_LOCK, out, &out);

  if (!rc)
    printf("lock is %s\n", out[0] ? "enabled" : "disabled");

  return rc != 0;
}

// Vendor command to set the console lock
static int VcSetLock(TrunksDBusProxy* proxy, base::CommandLine* cl)
{
  std::string out;
  uint32_t rc = VendorCommand(proxy, VENDOR_CC_SET_LOCK, out, &out);

  if (!rc)
    printf("lock is enabled\n");

  return rc != 0;
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  brillo::InitLog(brillo::kLogToStderr);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();

  if (cl->HasSwitch(kVerbose)) {
    verbose = 1;
  }

  TrunksDBusProxy proxy;
  if (!proxy.Init()) {
    LOG(ERROR) << "Failed to initialize dbus proxy.";
    return 1;
  }

  if (cl->HasSwitch(kRaw))
    return HandleRaw(&proxy, cl);

  if (cl->HasSwitch(kGetLock))
    return VcGetLock(&proxy, cl);

  if (cl->HasSwitch(kSetLock))
    return VcSetLock(&proxy, cl);

  PrintUsage();
  return 1;
}
