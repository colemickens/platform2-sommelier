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
#include <openssl/sha.h>

#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_util.h>
#include <base/sys_byteorder.h>
#include <brillo/syslog_logging.h>

#include "trunks/trunks_dbus_proxy.h"

namespace {

using trunks::TrunksDBusProxy;

// Commands we support
constexpr char kForce[] = "force";
constexpr char kGetLock[] = "get_lock";
constexpr char kRaw[] = "raw";
constexpr char kSetLock[] = "set_lock";
constexpr char kSysInfo[] = "sysinfo";
constexpr char kUpdate[] = "update";
constexpr char kVerbose[] = "verbose";

// Maximum image update block size expected by Cr50.
// Equals to SIGNED_TRANSFER_SIZE in src/platform/ec/chip/g/update_fw.h
static const uint32_t kTransferSize = 1024;

static int verbose;

void PrintUsage() {
  printf("Usage:\n");
  printf("  trunks_send --%s\n", kGetLock);
  printf("  trunks_send --%s\n", kSetLock);
  printf("  trunks_send --%s\n", kSysInfo);
  printf("  trunks_send --%s XX [XX ..]\n", kRaw);
  printf("  trunks_send [--%s] --%s <bin file>\n", kForce, kUpdate);
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

// Vendor-specific command codes
#define TPM_CC_VENDOR_CR50      0x0000

// This needs to be used to be backwards compatible with older Cr50 versions.
#define CR50_EXTENSION_COMMAND    0xbaccd00a
#define CR50_EXTENSION_FW_UPGRADE 4

// Cr50 vendor-specific subcommand codes. 16 bits available.
enum vendor_cmd_cc {
  VENDOR_CC_POST_RESET = 7,
  VENDOR_CC_GET_LOCK = 16,
  VENDOR_CC_SET_LOCK = 17,
  VENDOR_CC_SYSINFO = 18,
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
                              std::string* output,
                              bool extendedCommandMode = false) {
  // Pack up the header and the input
  struct TpmCmdHeader header;
  header.tag = base::HostToNet16(trunks::TPM_ST_NO_SESSIONS);
  header.size = base::HostToNet32(sizeof(header) + input.size());
  if (extendedCommandMode)
    header.code = base::HostToNet32(CR50_EXTENSION_COMMAND);
  else
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

//
// A convenience structure which allows to group together various revision
// fields of the header created by the signer.
//
// These fields are compared when deciding if versions of two images are the
// same or when deciding which one of the available images to run.
//
// Originally defined in src/platform/ec/chip/g/upgrade_fw.h
//
struct SignedHeaderVersion {
  uint32_t minor;
  uint32_t major;
  uint32_t epoch;
};

//
// Response to the connection establishment request.
//
// All protocol versions starting with version 2 respond to the very first
// packet with an 8 byte or larger response, where the first 4 bytes are a
// version specific data, and the second 4 bytes - the protocol version
// number.
//
// Originally defined in src/platform/ec/chip/g/upgrade_fw.h
//
struct FirstResponsePdu {
  uint32_t return_value;

  // The below fields are present in versions 2 and up.
  uint32_t protocol_version;

  // The below fields are present in versions 3 and up.
  uint32_t  backup_ro_offset;
  uint32_t  backup_rw_offset;

  // The below fields are present in versions 4 and up.
  // Versions of the currently active RO and RW sections.
  SignedHeaderVersion shv[2];

  // The below fields are present in versions 5 and up
  // keyids of the currently active RO and RW sections.
  uint32_t keyid[2];
};

struct UpdatePduHeader {
  uint32_t pdu_digest;
  uint32_t pdu_base_offset;
};

//
// Cr50 image header.
//
// Based on SignedHeader defined in src/platform/ec/chip/g/signed_header.h
//
struct EssentialHeader {
  uint32_t magic;
  uint32_t padding0[201];
  uint32_t image_size;
  uint32_t padding1[12];
  uint32_t epoch;
  uint32_t major;
  uint32_t minor;
};

//
// Wraps a block of image into a Vendor Command PDU and sends it to the device.
//
// Wrapping includes creating a header containing the digest of the entire PDU
// and the offset to program the PDU contents int the device's flash.
//
// |data| points to the entire firmware image containing RO and RW sections.
// |data_offset| is the offset into the image and into the flash memory, and
// |block_size| is the number of bytes to be tranferred with this block.
//
// Returns true on success, false on error.
//
static bool TransferBlock(TrunksDBusProxy* proxy,
                          const char* data,
                          size_t data_offset,
                          size_t block_size) {
  uint8_t digest[SHA_DIGEST_LENGTH];
  UpdatePduHeader updu;
  SHA_CTX shaCtx;
  std::string response;

  printf("sending 0x%zx bytes to offset %#zx\n", block_size, data_offset);

  updu.pdu_base_offset = base::NetToHost32(data_offset);
  SHA1_Init(&shaCtx);
  SHA1_Update(&shaCtx, &updu.pdu_base_offset, sizeof(updu.pdu_base_offset));
  SHA1_Update(&shaCtx, data + data_offset, block_size);
  SHA1_Final(digest, &shaCtx);

  memcpy(&updu.pdu_digest, digest, sizeof(updu.pdu_digest));

  std::string request =
      std::string(reinterpret_cast<char*>(&updu), sizeof(updu)) +
      std::string(data + data_offset, block_size);

  uint32_t rv = VendorCommand(proxy, CR50_EXTENSION_FW_UPGRADE, request,
                              &response, true);
  if (rv) {
    LOG(ERROR) << "Failed to transfer image block, got 0x" << std::hex << rv;
    return false;
  }

  if (response.size() != 1) {
    LOG(ERROR) << "Unexpected return size " << response.size();
    return false;
  }

  if (response.data()[0]) {
    rv = response.data()[0];
    LOG(ERROR) << "Error " << rv;
    return false;
  }

  return true;
}

//
// Sends to the TPM the first transfer PDU, it is just 8 bytes of zeros. Verify
// the expected response (which is of FirstResponsePdu structure).
//
// Returns true on success, false on error.
//
static bool SetupConnection(TrunksDBusProxy* proxy,
                            FirstResponsePdu* rpdu) {
  // Connection setup is triggered by 8 bytes of zeros.
  std::string request(8, 0);
  std::string response;

  uint32_t rv = VendorCommand(proxy, CR50_EXTENSION_FW_UPGRADE,
                              request, &response, true);
  if (rv) {
    LOG(ERROR) << "Failed to set up connection, got 0x" << std::hex << rv;
    return false;
  }

  // We got something. Check for errors.
  if (response.size() < sizeof(FirstResponsePdu)) {
    LOG(ERROR) << "Unexpected response size " << response.size();
    return false;
  }

  // Let's unmarshal the response
  memcpy(rpdu, response.data(), std::min(sizeof(*rpdu), response.size()));

  rpdu->return_value = base::NetToHost32(rpdu->return_value);
  if (rpdu->return_value) {
    LOG(ERROR) << "Target reporting error 0x" << std::hex << rpdu->return_value;
    return false;
  }

  rpdu->protocol_version = base::NetToHost32(rpdu->protocol_version);
  if (rpdu->protocol_version < 5) {
    LOG(ERROR) << "Unsupported protocol version " << rpdu->protocol_version;
    return false;
  }
  printf("protocol version: %d\n", rpdu->protocol_version);
  rpdu->backup_ro_offset = base::NetToHost32(rpdu->backup_ro_offset);
  rpdu->backup_rw_offset = base::NetToHost32(rpdu->backup_rw_offset);

  for (int i = 0; i < arraysize(FirstResponsePdu::shv); i++) {
    rpdu->shv[i].minor = base::NetToHost32(rpdu->shv[i].minor);
    rpdu->shv[i].major = base::NetToHost32(rpdu->shv[i].major);
    rpdu->shv[i].epoch = base::NetToHost32(rpdu->shv[i].epoch);
  }

  printf("offsets: backup RO at %#x, backup RW at %#x\n",
         rpdu->backup_ro_offset, rpdu->backup_rw_offset);
  return true;
}

//
// Compares version fields in the header of the new image to the versions
// running on the target. Returns true if the new image is newer.
//
static bool ImageIsNewer(const EssentialHeader &header,
                         const SignedHeaderVersion &shv) {

  if (header.epoch != shv.epoch)
    return header.epoch > shv.epoch;
  if (header.major != shv.major)
    return header.major > shv.major;
  return header.minor > shv.minor;
}

//
// Updates RO or RW section of the Cr50 image on the device.
// A section is updated only if it's newer than the one currently on the
// device, or if |force| is set to true.
//
// |update_image| is the entire 512K file produced by the builder,
// |section_offset| is the offset of either inactive RO or inactive RW on
// the device, |shv| communicates this section's version retrieved from the
// device.
//
// Returns true on success, false on error. Skipping an update if the current
// version is not older than the one in |update_image| is considered a success.
//
static bool TransferSection(TrunksDBusProxy* proxy,
                            std::string &update_image,
                            uint32_t section_offset,
                            const SignedHeaderVersion &shv,
                            bool force) {
  EssentialHeader header;

  // Try reading the header into the structure.
  if ((section_offset + sizeof(EssentialHeader)) > update_image.size()) {
    LOG(ERROR) << "Header at offset 0x" << std::hex << section_offset
               << " does not fit into the image of "
               << std::dec << update_image.size() << " bytes";
    return false;
  }
  memcpy(&header, update_image.data() + section_offset, sizeof(header));

  if (header.magic != 0xffffffff) {
    LOG(ERROR) << "Wrong magic value 0x" << std::hex << header.magic
               << " at offset 0x" << std::hex << section_offset;
    return false;
  }

  if (header.image_size > (update_image.size() - section_offset)) {
    LOG(ERROR) << "Wrong section size 0x" << std::hex << header.image_size
               << " at offset 0x" << std::hex << section_offset;
    return false;
  }

  printf("Offset %#x file at %d.%d.%d device at %d.%d.%d, section size %d\n",
         section_offset,
         header.epoch, header.major, header.minor,
         shv.epoch, shv.major, shv.minor,
         header.image_size);
  if (!force && !ImageIsNewer(header, shv)) {
    printf("Skipping update\n");
    return true;
  }

  // Transfer section, one block at a time.
  size_t block_size;
  for (uint32_t transferred = 0; transferred < header.image_size;
       transferred += block_size) {
    block_size = std::min(header.image_size - transferred, kTransferSize);
    if (!TransferBlock(proxy, update_image.data(), section_offset + transferred,
                       block_size)) {
      return false;
    }
  }

  return true;
}

//
// Updates the Cr50 image on the device. |update_image| contains the entire
// new Cr50 image.
// Each of the Cr50 sections is updated only if it's newer than the one
// currently on the device, or if |force| is set to true. Otherwise the
// session is skipped. The information about the section offsets and current
// versions is taken from the response to the connection request |rpdu| received
// from the device earlier.
//
// Returns the number of successfully updated sections (including skipped), or
// a negative value in case of error.
//
static int TransferImage(TrunksDBusProxy* proxy,
                         std::string &update_image,
                         const FirstResponsePdu &rpdu,
                         bool force) {
  int num_txed_sections = 0;
  uint32_t section_offsets[] = {rpdu.backup_ro_offset, rpdu.backup_rw_offset};
  int index;

  //
  // The cr50 will not accept lower addresses after higher addresses for 60
  // seconds. Decide what section needs to be transferred first.
  //

  index = section_offsets[0] > section_offsets[1] ? 1 : 0;
  for (int i = 0; i < arraysize(section_offsets); i++) {
    if (!TransferSection(proxy, update_image, section_offsets[index],
                         rpdu.shv[index], force)) {
      if (!force) {
        return -1;
      }
    } else {
      num_txed_sections++;
    }
    index = (index + 1) % arraysize(section_offsets);
  }

  return num_txed_sections;
}

enum UpdateStatus {
  UpdateSuccess = 0,
  UpdateError = 1,
  UpdateCancelled = 2
};

// Updathe the Cr50 image on the device.
static UpdateStatus HandleUpdate(TrunksDBusProxy* proxy,
                                 base::CommandLine* cl) {
  if (cl->GetArgs().size() != 1) {
    LOG(ERROR) << "A single image file name must be provided.";
    return UpdateError;
  }

  base::FilePath filename(cl->GetArgs()[0]);

  std::string update_image;
  if (!base::ReadFileToString(filename, &update_image)) {
    LOG(ERROR) << "Failed to read " << filename.value();
    return UpdateError;
  }

  FirstResponsePdu rpdu;
  if (!SetupConnection(proxy, &rpdu)) {
    return UpdateError;
  }

  // Cr50 images with RW versoin below 0.0.19 process updates differently,
  // and as such require special treatment.
  bool running_pre_19 =
    rpdu.shv[1].minor < 19 &&
    rpdu.shv[1].major == 0 &&
    rpdu.shv[1].epoch == 0;

  if (running_pre_19 && !cl->HasSwitch(kForce)) {
    printf("Not updating from RW 0.0.%d, use --force if necessary\n",
           rpdu.shv[1].minor);
    return UpdateCancelled;
  }

  int rv = TransferImage(proxy, update_image, rpdu, cl->HasSwitch(kForce));

  if (rv < 0) {
    return UpdateError;
  }

  // Positive rv indicates that some sections were transferred and a Cr50
  // reboot is required. RW Cr50 versions below 0.0.19 require a posted reset
  // to switch to the new image.
  if (rv > 0 && running_pre_19) {
    std::string dummy;

    LOG(INFO) << "Will post a reset request.";

    if (VendorCommand(proxy, VENDOR_CC_POST_RESET, dummy, &dummy, true)) {
      LOG(ERROR) << "Failed to post a reset request.";
      return UpdateError;
    }
  }

  return UpdateSuccess;
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

static const char *key_type(uint32_t key_id)
{
  // It is a mere convention, but all prod keys are required to have key
  // IDs such that bit D2 is set, and all dev keys are required to have
  // key IDs such that bit D2 is not set.
  if (key_id & (1 << 2))
    return "prod";
  else
    return "dev";
}

// SysInfo command:
// There are no input args.
// Output is this struct, all fields in network order.
struct sysinfo_s {
  uint32_t ro_keyid;
  uint32_t rw_keyid;
  uint32_t dev_id0;
  uint32_t dev_id1;
} __attribute__((packed));

static int VcSysInfo(TrunksDBusProxy* proxy, base::CommandLine* cl)
{
  std::string out;
  uint32_t rc = VendorCommand(proxy, VENDOR_CC_SYSINFO, out, &out);

  if (rc)
    return 1;

  if (out.size() != sizeof(struct sysinfo_s)) {
    LOG(ERROR) << "TPM response was too short!";
    return 1;
  }

  struct sysinfo_s sysinfo;
  memcpy(&sysinfo, out.c_str(), out.size());
  sysinfo.ro_keyid = base::NetToHost32(sysinfo.ro_keyid);
  sysinfo.rw_keyid = base::NetToHost32(sysinfo.rw_keyid);
  sysinfo.dev_id0 = base::NetToHost32(sysinfo.dev_id0);
  sysinfo.dev_id1 = base::NetToHost32(sysinfo.dev_id1);

  printf("RO keyid:    0x%08x (%s)\n",
         sysinfo.ro_keyid, key_type(sysinfo.ro_keyid));
  printf("RW keyid:    0x%08x (%s)\n",
         sysinfo.rw_keyid, key_type(sysinfo.rw_keyid));
  printf("DEV_ID:      0x%08x 0x%08x\n",
         sysinfo.dev_id0, sysinfo.dev_id1);

  return 0;
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

  if (cl->HasSwitch(kSysInfo))
    return VcSysInfo(&proxy, cl);

  if (cl->HasSwitch(kUpdate))
    return HandleUpdate(&proxy, cl);

  PrintUsage();
  return 1;
}
