// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <base/logging.h>
#include <base/macros.h>
#include <base/rand_util.h>
#include <base/strings/stringprintf.h>
#include <base/strings/string_number_conversions.h>
#include <base/time/time.h>
#include <base/timer/elapsed_timer.h>
#include <brillo/syslog_logging.h>
#include <crypto/random.h>

#include "u2fd/g2f_tools/g2f_client.h"

namespace {

constexpr size_t kFrameSize = u2f::kU2fReportSize;

brillo::Blob GetRandomData(size_t size) {
  brillo::Blob blob(size);
  base::RandBytes(blob.data(), size);
  return blob;
}

}  // namespace

namespace g2f_client {

struct FrameInitHeader {
  HidDevice::Cid cid;
  uint8_t cmd;
  uint8_t bcnth;
  uint8_t bcntl;
} __attribute__((__packed__));

struct FrameInit {
  FrameInitHeader header;
  uint8_t data[kFrameSize - sizeof(FrameInitHeader)];

  constexpr size_t PayloadSize() const {
    return (static_cast<size_t>(header.bcnth) << 8) + header.bcntl;
  }
  constexpr static size_t MaxDataSize() {
    return arraysize(data);
  }
  constexpr static size_t DataFits(size_t all_data_size) {
    return std::min(all_data_size, MaxDataSize());
  }
} __attribute__((__packed__));
static_assert(sizeof(FrameInit) == kFrameSize, "Bad FromInit size");

struct FrameContHeader {
  HidDevice::Cid cid;
  uint8_t seq;
} __attribute__((__packed__));

struct FrameCont {
  FrameContHeader header;
  uint8_t data[kFrameSize - sizeof(FrameContHeader)];

  constexpr static size_t MaxDataSize() {
    return arraysize(data);
  }
  constexpr static size_t DataFits(size_t all_data_size) {
    return std::min(all_data_size, MaxDataSize());
  }
} __attribute__((__packed__));
static_assert(sizeof(FrameCont) == kFrameSize, "Bad FrameCont size");

struct FrameBlob {
  uint8_t report_id;
  union {
    HidDevice::Cid cid;
    FrameInit init;
    FrameCont cont;
  } frame;

  constexpr bool IsInit() const {
    return frame.init.header.cmd & u2f::kFrameTypeInit;
  }
  constexpr size_t MaxDataSize() const {
    return IsInit() ? FrameInit::MaxDataSize() : FrameCont::MaxDataSize();
  }
  constexpr size_t DataFits(size_t all_data_size) const {
    return IsInit() ?
           FrameInit::DataFits(all_data_size) :
           FrameCont::DataFits(all_data_size);
  }
  constexpr uint8_t* Data() {
    return IsInit() ? frame.init.data : frame.cont.data;
  }
  constexpr const uint8_t* Data() const {
    return IsInit() ? frame.init.data : frame.cont.data;
  }
  constexpr unsigned PrintableCmdValue() const {
    return static_cast<unsigned>(frame.init.header.cmd);
  }
} __attribute__((__packed__));
static_assert(sizeof(FrameBlob) == kFrameSize + 1, "Bad FrameBlob size");

HidDevice::HidDevice(const std::string& path)
    : path_(path) {}

HidDevice::~HidDevice() {
  Close();
}

bool HidDevice::Open() {
  if (!IsOpened()) {
    dev_ = hid_open_path(path_.c_str());
    if (!IsOpened()) {
      LOG(ERROR) << "Failed to open " << path_;
      return false;
    }
  }
  return true;
}

void HidDevice::Close() {
  if (dev_) {
    hid_close(dev_);
    dev_ = nullptr;
  }
}

bool HidDevice::SendRequest(const Cid& cid, uint8_t cmd,
                            const brillo::Blob& payload) {
  size_t size = payload.size();
  if (size > UINT16_MAX) {
    LOG(ERROR) << "Too large payload (" << size << ") for cmd "
               << cmd;
    return false;
  }
  if (!dev_) {
    LOG(ERROR) << "SendRequest on closed device.";
    return false;
  }

  FrameBlob blob;
  blob.report_id = 0;
  blob.frame.init.header.cid = cid;
  blob.frame.init.header.cmd = cmd | u2f::kFrameTypeInit;
  blob.frame.init.header.bcnth = static_cast<uint8_t>((size >> 8) & 0xFFu);
  blob.frame.init.header.bcntl = static_cast<uint8_t>(size & 0xFFu);

  size_t data_size = FrameInit::DataFits(size);
  const uint8_t* data_start = payload.data();
  std::copy(data_start, data_start + data_size, blob.frame.init.data);
  uint8_t seq = 0;

  do {
    if (!WriteBlob(blob)) {
      return false;
    }
    size -= data_size;
    data_start += data_size;

    blob.frame.cont.header.cid = cid;
    blob.frame.cont.header.seq = seq++;
    data_size = FrameCont::DataFits(size);
    std::copy(data_start, data_start + data_size, blob.frame.cont.data);
  } while (size);
  return true;
}

bool HidDevice::RecvResponse(const Cid& cid, uint8_t* cmd,
                             brillo::Blob* payload, int timeout_ms) {
  CHECK(cmd);
  CHECK(payload);

  base::ElapsedTimer timer;
  FrameBlob blob;
  bool wait_for_init = true;
  size_t size = 0;
  uint8_t seq = 0;
  uint8_t* payload_ptr = nullptr;

  if (!dev_) {
    LOG(ERROR) << "RecvResponse on closed device.";
    return false;
  }

  do {
    if (!ReadBlob(&blob, timeout_ms)) {
      return false;
    }

    if (blob.frame.cid.value() != cid.value()) {
      LOG(WARNING) << "Unexpected response from cid " << blob.frame.cid.value();
      continue;
    }

    if (wait_for_init) {
      if (!blob.IsInit()) {
        LOG(WARNING) << "Unexpected CONT from cid " << cid.value() << ": "
                     << blob.PrintableCmdValue();
        continue;
      }
      *cmd = blob.frame.init.header.cmd & ~u2f::kFrameTypeMask;
      size = blob.frame.init.PayloadSize();
      payload->resize(size);
      payload_ptr = payload->data();
      wait_for_init = false;
    } else {
      if (blob.IsInit()) {
        LOG(ERROR) << "Unexpected INIT from cid " << cid.value() << ": "
                   << blob.PrintableCmdValue();
        return false;
      }
      if (blob.frame.cont.header.seq != seq++) {
        LOG(ERROR) << "Unexpected SEQ from cid " << cid.value() << ": "
                   << static_cast<unsigned>(blob.frame.cont.header.seq)
                   << " instead of " << static_cast<unsigned>(seq-1);
        return false;
      }
    }
    size_t data_size = blob.DataFits(size);
    const uint8_t* data_start = blob.Data();
    std::copy(data_start, data_start + data_size, payload_ptr);
    payload_ptr += data_size;
    size -= data_size;
  } while ((wait_for_init || size) &&
           (timeout_ms < 0 ||
            timer.Elapsed().InMilliseconds() < timeout_ms));
  return !(wait_for_init || size);
}

bool HidDevice::WriteBlob(const FrameBlob& blob) {
  const unsigned char* data =
      static_cast<const unsigned char*>(&blob.report_id);
  VLOG(3) << "HID Send Frame "
          << base::HexEncode(data + 1, kFrameSize);
  return CheckDeviceError("hid_write", hid_write(dev_, data, sizeof(FrameBlob)),
                          sizeof(FrameBlob));
}

bool HidDevice::ReadBlob(FrameBlob* blob, int timeout_ms) {
  CHECK(blob);
  blob->report_id = 0;
  unsigned char* data = static_cast<unsigned char*>(&blob->report_id) + 1;
  if (!CheckDeviceError("hid_read_timeout",
                        hid_read_timeout(dev_, data, kFrameSize, timeout_ms),
                        kFrameSize)) {
    return false;
  }
  VLOG(3) << "HID Recv Frame " << base::HexEncode(data, kFrameSize);
  return true;
}

bool HidDevice::CheckDeviceError(const std::string& func, int res,
                                 int expected) {
  if (res == expected) {
    return true;
  }
  LOG(ERROR) << func << " for " << path_
              << " failed (" << res << " != " << expected << "): "
              << hid_error(dev_);
  return false;
}

bool U2FHid::Command::CheckSuccess(const std::string& descr) const {
  if (!IsError()) {
    return true;
  }
  LOG(ERROR) << descr << " failed: " << static_cast<unsigned>(ErrorCode())
             << " (" << ErrorName() << ")";
  return false;
}

constexpr uint8_t U2FHid::Command::ErrorCode() const {
  if (!IsError()) {
    return static_cast<uint8_t>(ErrorCode::kNone);
  }

  return payload.empty() ? static_cast<uint8_t>(ErrorCode::kOther) : payload[0];
}

std::string U2FHid::Command::Description() const {
  return base::StringPrintf("%u (%s) [%zu bytes]",
                            cmd, CommandName().c_str(),
                            payload.size());
}

std::string U2FHid::Command::FullDump() const {
  return Description() + ": " +
         base::HexEncode(payload.data(), payload.size());
}

std::string U2FHid::Command::CommandName() const {
  switch (static_cast<CommandCode>(cmd)) {
    case CommandCode::kPing:
      return "Ping";
    case CommandCode::kMsg:
      return "Msg";
    case CommandCode::kLock:
      return "Lock";
    case CommandCode::kVendorSysInfo:
      return "VendorSysInfo";
    case CommandCode::kInit:
      return "Init";
    case CommandCode::kWink:
      return "Wink";
    case CommandCode::kError:
      return "Error";
  }
  return "?";
}

std::string U2FHid::Command::ErrorName() const {
  switch (static_cast<U2FHid::ErrorCode>(ErrorCode())) {
    case ErrorCode::kNone:
      return "None";
    case ErrorCode::kInvalidCmd:
      return "InvalidCmd";
    case ErrorCode::kInvalidPar:
      return "InvalidPar";
    case ErrorCode::kInvalidLen:
      return "InvalidLen";
    case ErrorCode::kInvalidSeq:
      return "InvalidSeq";
    case ErrorCode::kMsgTimeout:
      return "MsgTimeout";
    case ErrorCode::kChannelBusy:
      return "ChannelBusy";
    case ErrorCode::kLockRequired:
      return "LockRequired";
    case ErrorCode::kInvalidCid:
      return "InvalidCid";
    case ErrorCode::kOther:
      return "Other";
  }
  return "?";
}

U2FHid::U2FHid(HidDevice* hid_device)
    : hid_device_(hid_device) {}

bool U2FHid::RawCommand(const Command& request, Command* response) {
  CHECK(response);
  if (!hid_device_) {
    LOG(ERROR) << "No hid device provided";
    return false;
  }
  if (!hid_device_->Open()) {
    return false;
  }
  VLOG(2) << "U2F SEND " << request.FullDump();
  if (!hid_device_->SendRequest(cid_, request.cmd, request.payload)) {
    return false;
  }
  if (!hid_device_->RecvResponse(cid_, &response->cmd, &response->payload,
                                 timeout_ms_)) {
    return false;
  }
  VLOG(2) << "U2F RECV " << response->FullDump();
  return true;
}

bool U2FHid::GetSuccessfulResponse(const Command& request, Command* response) {
  if (!RawCommand(request, response)) {
    LOG(ERROR) << "Sending " << request.Description() << " failed";
    return false;
  }
  return response->CheckSuccess("Command");
}

bool U2FHid::Init(bool force_realloc) {
  struct InitResponse {
    uint8_t nonce[u2f::kInitNonceSize];
    HidDevice::Cid cid;
    Version version;
    uint8_t caps;
  } __attribute__((__packed__));

  if (Initialized()) {
    if (!force_realloc) {
      return true;
    }
    VLOG(1) << "Forcing re-initialization.";
    cid_ = HidDevice::kCidBroadcast;
  }

  Command req = { CommandCode::kInit, GetRandomData(u2f::kInitNonceSize) };
  Command rsp;
  InitResponse parsed_response;
  if (req.payload.size() != u2f::kInitNonceSize) {
    LOG(ERROR) << "Failed to generate nonce, size = " << req.payload.size();
    return false;
  }
  if (!GetSuccessfulResponse(req, &rsp)) {
    return false;
  }
  if (rsp.payload.size() < sizeof(InitResponse)) {
    LOG(ERROR) << "Bad INIT response size: " << rsp.payload.size();
    return false;
  }
  std::copy(rsp.payload.data(), rsp.payload.data() + sizeof(InitResponse),
            parsed_response.nonce);
  if (std::memcmp(parsed_response.nonce, req.payload.data(),
                  u2f::kInitNonceSize)) {
    LOG(ERROR) << "INIT nonce mismatch";
    return false;
  }
  if (parsed_response.cid.IsBroadcast()) {
    LOG(ERROR) << base::StringPrintf("Bad cid from init: %#08x",
                                     parsed_response.cid.value());
    return false;
  }

  caps_ = parsed_response.caps;
  version_ = parsed_response.version;
  cid_ = parsed_response.cid;
  VLOG(2) << base::StringPrintf("INIT: Version = %u.%u.%u.%u",
                                version_.protocol, version_.major,
                                version_.minor, version_.build);
  VLOG(2) << base::StringPrintf("INIT: Caps = %#x", caps_);
  VLOG(1) << base::StringPrintf("Using cid %u (%#08x)", cid_.value(),
                                cid_.value());
  return true;
}

bool U2FHid::Lock(uint8_t lock_timeout_seconds) {
  if (!Init(false /* force_realloc */)) {
    return false;
  }
  if (lock_timeout_seconds > 10) {
    LOG(WARNING) << "Too large Lock timeout: "
                 << static_cast<unsigned>(lock_timeout_seconds);
  }

  Command req = { CommandCode::kLock, { lock_timeout_seconds } };
  Command rsp;
  if (!GetSuccessfulResponse(req, &rsp)) {
    return false;
  }
  if (!rsp.payload.empty()) {
    LOG(ERROR) << "Lock response contains unexpected data";
    return false;
  }
  return true;
}

bool U2FHid::Msg(const brillo::Blob& request, brillo::Blob* response) {
  CHECK(response);
  if (!Init(false /* force_realloc */)) {
    return false;
  }
  Command req = { CommandCode::kMsg, request };
  Command rsp;
  if (!GetSuccessfulResponse(req, &rsp)) {
    return false;
  }
  *response = rsp.payload;
  return true;
}

bool U2FHid::Ping(size_t size) {
  if (!Init(false /* force_realloc */)) {
    return false;
  }
  Command req = { CommandCode::kPing, GetRandomData(size) };
  Command rsp;
  if (!GetSuccessfulResponse(req, &rsp)) {
    return false;
  }
  if (rsp.payload != req.payload) {
    LOG(ERROR) << "Ping response data mismatch";
    return false;
  }
  return true;
}

bool U2FHid::Wink() {
  if (!Init(false /* force_realloc */)) {
    return false;
  }
  Command req = { CommandCode::kWink, {} };
  Command rsp;
  if (!GetSuccessfulResponse(req, &rsp)) {
    return false;
  }
  if (!rsp.payload.empty()) {
    LOG(ERROR) << "Wink response contains unexpected data";
    return false;
  }
  return true;
}

}  // namespace g2f_client
