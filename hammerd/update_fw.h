// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains structures used to facilitate EC firmware updates
// over USB. Note that many contents in this file are copied from EC overlay,
// and it might be trickier to include them directly.
//
// The firmware update protocol consists of two phases: connection
// establishment and actual image transfer.
//
// Image transfer is done in 1K blocks. The host supplying the image
// encapsulates blocks in PDUs by prepending a header including the flash
// offset where the block is destined and its digest.
//
// The EC device responds to each PDU with a confirmation which is 1 byte
// response. Zero value means success, non zero value is the error code
// reported by EC.
//
// To establish the connection, the host sends a different PDU, which
// contains no data and is destined to offset 0. Receiving such a PDU
// signals the EC that the host intends to transfer a new image.
//
// The connection establishment response is described by the
// FirstResponsePDU structure below.

#ifndef HAMMERD_UPDATE_FW_H_
#define HAMMERD_UPDATE_FW_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>
#include <gtest/gtest_prod.h>

#include "hammerd/fmap_utils.h"
#include "hammerd/usb_utils.h"

namespace hammerd {

constexpr int kUpdateProtocolVersion = 6;
constexpr uint32_t kUpdateDoneCmd = 0xB007AB1E;
constexpr uint32_t kUpdateExtraCmd = 0xB007AB1F;

enum class FirstResponsePDUHeaderType {
  kCR50 = 0,
  kCommon = 1,
};

// The extra vendor subcommand.
enum class UpdateExtraCommand : uint16_t {
  kImmediateReset = 0,
  kJumpToRW = 1,
  kStayInRO = 2,
  kUnlockRW = 3,
  kUnlockRollback = 4,
  kInjectEntropy = 5,
  kPairChallenge = 6,
};

const char* ToString(UpdateExtraCommand subcommand);

// This is the frame format the host uses when sending update PDUs over USB.
//
// The PDUs are up to 1K bytes in size, they are fragmented into USB chunks of
// 64 bytes each and reassembled on the receive side before being passed to
// the flash update function.
//
// The flash update function receives the unframed PDU body, and puts its reply
// into the same buffer the PDU was in.
struct UpdateFrameHeader {
  uint32_t block_size;  // Total frame size, including this field.
  uint32_t block_digest;
  uint32_t block_base;
  UpdateFrameHeader() : UpdateFrameHeader(0, 0, 0) {}
  UpdateFrameHeader(uint32_t size, uint32_t digest, uint32_t base)
      : block_size(htobe32(size)),
        block_digest(htobe32(digest)),
        block_base(htobe32(base)) {}
};

// Response to the connection establishment request.
//
// When responding to the very first packet of the update sequence, the
// original USB update implementation was responding with a four byte value,
// just as to any other block of the transfer sequence.
//
// It became clear that there is a need to be able to enhance the update
// protocol, while staying backwards compatible.
//
// All newer protocol versions (starting with version 2) respond to the very
// first packet with an 8 byte or larger response, where the first 4 bytes are
// a version specific data, and the second 4 bytes - the protocol version
// number.
//
// This way the host receiving of a four byte value in response to the first
// packet is considered an indication of the target running the 'legacy'
// protocol, version 1. Receiving of an 8 byte or longer response would
// communicates the protocol version in the second 4 bytes.
struct FirstResponsePDU {
  uint32_t return_value;

  // The below fields are present in versions 2 and up.
  // Type of header following (one of first_response_pdu_header_type)
  uint16_t header_type;
  uint16_t protocol_version;  // Must be kUpdateProtocolVersion
  uint32_t maximum_pdu_size;  // Maximum PDU size
  uint32_t flash_protection;  // Flash protection status
  uint32_t offset;            // Offset of the other region
  char version[32];           // Version string of the other region
  int32_t min_rollback;       // Minimum rollback version that RO will accept
  uint32_t key_version;       // RO public key version
};

enum class SectionName {
  RO,
  RW,
  Invalid,
};
const char* ToString(SectionName name);
SectionName OtherSection(SectionName name);

// This array describes all four sections of the new image.
struct SectionInfo {
  SectionName name;
  uint32_t offset;
  uint32_t size;
  char version[32];
  int32_t rollback;
  int32_t key_version;
  explicit SectionInfo(SectionName name);
  SectionInfo(SectionName name,
              uint32_t offset,
              uint32_t size,
              const char* version,
              int32_t rollback,
              int32_t key_version);
  friend bool operator==(const SectionInfo& lhs, const SectionInfo& rhs);
  friend bool operator!=(const SectionInfo& lhs, const SectionInfo& rhs);
};

class FirmwareUpdaterInterface {
 public:
  virtual ~FirmwareUpdaterInterface() = default;
  virtual bool LoadImage(const std::string& image) = 0;
  virtual bool TryConnectUSB() = 0;
  virtual void CloseUSB() = 0;
  virtual bool SendFirstPDU() = 0;
  virtual void SendDone() = 0;
  virtual bool InjectEntropy() = 0;

  virtual bool SendSubcommand(UpdateExtraCommand subcommand) = 0;
  virtual bool SendSubcommandWithPayload(UpdateExtraCommand subcommand,
                                         const std::string& cmd_body) = 0;
  virtual bool SendSubcommandReceiveResponse(UpdateExtraCommand subcommand,
                                             const std::string& cmd_body,
                                             void* resp,
                                             size_t resp_size) = 0;
  virtual bool TransferImage(SectionName section_name) = 0;
  virtual SectionName CurrentSection() const = 0;
  virtual bool NeedsUpdate(SectionName section_name) const = 0;
  virtual bool IsSectionLocked(SectionName section_name) const = 0;
  virtual bool UnLockSection(SectionName section_name) = 0;
};

// Implement the core logic of updating firmware.
// It contains the data of the original transfer_descriptor.
class FirmwareUpdater : public FirmwareUpdaterInterface {
 public:
  FirmwareUpdater();

  // Scans the new image and retrieve versions of RO and RW sections.
  bool LoadImage(const std::string& image) override;

  // Tries to connect to the USB endpoint during a period of time.
  bool TryConnectUSB() override;

  // Closes the connection to the USB endpoint.
  void CloseUSB() override;

  // Setups the connection with the EC firmware by sending the first PDU.
  // Returns true if successfully setup the connection.
  bool SendFirstPDU() override;

  // Indicates to the target that update image transfer has been completed. Upon
  // receipt of this message the target state machine transitions into the
  // 'rx_idle' state. The host may send an extension command to reset the target
  // after this.
  void SendDone() override;

  // Injects entropy into the hammer device.
  bool InjectEntropy() override;

  // Sends the external command through USB. The format of the payload is:
  //   4 bytes      4 bytes         4 bytes       2 bytes      variable size
  // +-----------+--------------+---------------+-----------+------~~~-------+
  // + total size| block digest |    EXT_CMD    | Vend. sub.|      data      |
  // +-----------+--------------+---------------+-----------+------~~~-------+
  //
  // Where 'Vend. sub' is the vendor subcommand, and data field is subcommand
  // dependent. The target tells between update PDUs and encapsulated vendor
  // subcommands by looking at the EXT_CMD value - it is kUpdateExtraCmd and
  // as such is guaranteed not to be a valid update PDU destination address.
  bool SendSubcommand(UpdateExtraCommand subcommand) override;
  bool SendSubcommandWithPayload(UpdateExtraCommand subcommand,
                                 const std::string& cmd_body) override;
  bool SendSubcommandReceiveResponse(UpdateExtraCommand subcommand,
                                     const std::string& cmd_body,
                                     void* resp,
                                     size_t resp_size) override;

  // Transfers the image to the target section.
  bool TransferImage(SectionName section_name) override;

  // Returns the current section that EC is running. One of "RO" or "RW".
  SectionName CurrentSection() const override;

  // Determines whether the given section needs updating.  Returns an accurate
  // answer regardless of which section is currently running.
  bool NeedsUpdate(SectionName section_name) const override;

  // Determines the section is locked or not.
  bool IsSectionLocked(SectionName section_name) const override;

  // Unlocks the section. Need to send "Reset" command afterward.
  bool UnLockSection(SectionName section_name) override;

 protected:
  // Used in unit tests to inject mocks.
  FirmwareUpdater(std::unique_ptr<UsbEndpoint> uep,
                  std::unique_ptr<FmapInterface> fmap);

  // Fetches the version of the currently-running section.
  bool FetchVersion();

  // Transfers the data of the target section.
  bool TransferSection(const uint8_t* data_ptr,
                       uint32_t section_addr,
                       size_t data_len);

  // Transfers a block of data.
  bool TransferBlock(UpdateFrameHeader* ufh,
                     const uint8_t* transfer_data_ptr,
                     size_t payload_size);

  // The USB endpoint to the hammer EC.
  std::unique_ptr<UsbEndpoint> uep_;
  // The fmap function interface.
  std::unique_ptr<FmapInterface> fmap_;
  // The information of the first response PDU.
  FirstResponsePDU targ_;
  // The version of the currently-running section.  Retrieved through USB
  // endpoint's configuration string descriptor as part of TryConnectUSB.
  std::string version_;
  // The image data to be updated.
  std::string image_;
  // The information of the RO and RW sections in the image data.
  std::vector<SectionInfo> sections_;

  friend class FirmwareUpdaterTest;
  FRIEND_TEST(FirmwareUpdaterTest, LoadImage);
  FRIEND_TEST(FirmwareUpdaterTest, TryConnectUSB_OK);
  FRIEND_TEST(FirmwareUpdaterTest, TryConnectUSB_FAIL);
  FRIEND_TEST(FirmwareUpdaterTest, TryConnectUSB_FetchVersion_Legacy);
  FRIEND_TEST(FirmwareUpdaterTest, TryConnectUSB_FetchVersion_FAIL);
  FRIEND_TEST(FirmwareUpdaterTest, SendFirstPDU);
  FRIEND_TEST(FirmwareUpdaterTest, SendDone);
  FRIEND_TEST(FirmwareUpdaterTest, SendSubcommand_InjectEntropy);
  FRIEND_TEST(FirmwareUpdaterTest, SendSubcommand_Reset);
  FRIEND_TEST(FirmwareUpdaterTest, TransferImage);
  FRIEND_TEST(FirmwareUpdaterTest, CurrentSection);
  FRIEND_TEST(FirmwareUpdaterTest, NeedsUpdate);

 private:
  DISALLOW_COPY_AND_ASSIGN(FirmwareUpdater);
};

}  // namespace hammerd
#endif  // HAMMERD_UPDATE_FW_H_
