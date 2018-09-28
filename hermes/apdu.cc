// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hermes/apdu.h"

#include <base/logging.h>
#include <base/strings/string_number_conversions.h>

namespace {

// Max data bytes for standard APDUs and extended APDUs.
// Note that the length limit for extended APDUs is not 65536 due
// to a limitation imposed by the Java Card platform.
constexpr size_t kMaxStandardDataSize = 255;
constexpr size_t kMaxExtendedDataSize = 32767;

// Number of bytes Lc and Le fields are in standard and extended APDUs.
// Note: Lc and Le must either both or neither be in extended form.
constexpr size_t kStandardLengthBytes = 1;
constexpr size_t kExtendedLengthBytes = 3;

constexpr size_t kHeaderSize = 4;  // CLA + INS + P1 + P2

}  // namespace

namespace hermes {

CommandApdu::CommandApdu(ApduClass cls,
                         ApduInstruction instruction,
                         bool is_extended_length,
                         uint16_t le)
    : is_extended_length_(is_extended_length),
      current_fragment_(0),
      le_(le),
      current_index_(0) {
  max_data_size_ =
      is_extended_length_ ? kMaxExtendedDataSize : kMaxStandardDataSize;
  // Note that 256 is valid for standard APDUs because an Le field of 0 is
  // interpreted to mean that Ne=256.
  if (!is_extended_length_ && 256 < le_) {
    LOG(INFO) << "CommandApdu created with Le of " << le_
              << ", but is not an extended length APDU. Setting Le to 256.";
    le_ = 256;
  } else if (kMaxExtendedDataSize < le_) {
    LOG(INFO) << "CommandApdu created with Le of " << le_
              << " but restrictions imposed by the Java Card platform requires "
              << "Le to fit into a signed 16 bit integer. Setting Le to 32767.";
    le_ = kMaxExtendedDataSize;
  }

  // Create APDU header.
  data_.push_back(static_cast<uint8_t>(cls));          // CLS
  data_.push_back(static_cast<uint8_t>(instruction));  // INS
  data_.push_back(0);                                  // P1
  data_.push_back(0);                                  // P2
}

void CommandApdu::AddData(const std::initializer_list<uint8_t>& data) {
  DCHECK_EQ(current_index_, 0);
  EnsureLcExists();
  data_.insert(data_.end(), data.begin(), data.end());
}

void CommandApdu::AddData(const std::vector<uint8_t>& data) {
  DCHECK_EQ(current_index_, 0);
  EnsureLcExists();
  data_.insert(data_.end(), data.begin(), data.end());
}

size_t CommandApdu::GetNextFragment(uint8_t** fragment) {
  DCHECK(fragment);
  if (current_index_ == data_.size()) {
    return 0;
  }

  size_t header_size = kHeaderSize;
  size_t length_size =
      is_extended_length_ ? kExtendedLengthBytes : kStandardLengthBytes;
  size_t lc_size = 0;
  // The APDU contains an Lc if it has any data.
  if (data_.size() > kHeaderSize) {
    lc_size += length_size;
  }
  header_size += lc_size;

  bool is_first_fragment = (current_index_ == 0);
  // Do not include APDU header in bytes_left calculation.
  current_index_ += is_first_fragment ? header_size : 0;
  size_t bytes_left = data_.size() - current_index_;
  size_t current_size = std::min(bytes_left, max_data_size_);
  bool is_last_fragment = (bytes_left == current_size);

  // Set up APDU header in-place.
  // If Lc is 0, the generated APDU should be either case 1 or 2.
  current_index_ -= header_size;
  data_[current_index_] = data_[0];
  data_[current_index_ + 1] = data_[1];
  data_[current_index_ + 2] =
      is_last_fragment ? kApduP1LastBlock : kApduP1MoreBlocks;
  data_[current_index_ + 3] = current_fragment_++;
  if (is_extended_length_) {
    data_[current_index_ + 4] = 0;
    data_[current_index_ + 5] = static_cast<uint8_t>(current_size & 0xFF);
    data_[current_index_ + 6] = static_cast<uint8_t>(current_size >> 8);
  } else {
    data_[current_index_ + 4] = static_cast<uint8_t>(current_size);
  }
  size_t le_size = 0;
  // Last fragment is the only one that will potentially have an Le field, as we
  // do not expect any response data until we send the entire command.
  if (is_last_fragment && le_) {
    le_size = length_size;
    data_.reserve(data_.size() + length_size);
    if (is_extended_length_) {
      data_.push_back(0);
      data_.push_back(static_cast<uint8_t>(le_ & 0xFF));
      data_.push_back(static_cast<uint8_t>(le_ >> 8));
    } else {
      data_.push_back(static_cast<uint8_t>(le_));
    }
  }
  // Add APDU header and (potentially) Le to size.
  current_size += header_size + le_size;
  *fragment = &data_[current_index_];
  current_index_ += current_size;
  VLOG(2) << "APDU fragment #" << current_fragment_ - 1 << " (" << current_size
          << " bytes): " << base::HexEncode(*fragment, current_size);
  return current_size;
}

void CommandApdu::EnsureLcExists() {
  if (data_.size() == kHeaderSize) {
    if (is_extended_length_) {
      data_.push_back(0);
      data_.push_back(0);
      data_.push_back(0);
    } else {
      data_.push_back(0);
    }
  }
}

//////////////////////////
// ResponseApdu Methods //
//////////////////////////

void ResponseApdu::AddData(const std::vector<uint8_t>& data) {
  RemoveStatusBytes();
  data_.insert(data_.end(), data.begin(), data.end());
}

void ResponseApdu::AddData(const uint8_t* data, size_t data_len) {
  RemoveStatusBytes();
  data_.insert(data_.end(), data, data + data_len);
}

std::vector<uint8_t> ResponseApdu::Release() {
  RemoveStatusBytes();
  return std::move(data_);
}

CommandApdu ResponseApdu::CreateGetMoreCommand(bool use_extended_length) const {
  uint8_t sw2 = 0;
  if (2 <= data_.size()) {
    sw2 = data_[data_.size() - 1];
  }
  return CommandApdu(ApduClass::STORE_DATA, ApduInstruction::GET_MORE_RESPONSE,
                     use_extended_length, sw2);
}

bool ResponseApdu::IsSuccessful() const {
  if (2 <= data_.size()) {
    return data_[data_.size() - 2] == STATUS_OK;
  }
  LOG(WARNING) << "Called IsSuccessful() on an empty ResponseApdu";
  return true;
}

bool ResponseApdu::WaitingForNextFragment() const {
  return (data_.empty() || data_.size() == 2) && IsSuccessful();
}

bool ResponseApdu::MorePayloadIncoming() const {
  if (2 <= data_.size()) {
    return data_[data_.size() - 2] == STATUS_MORE_RESPONSE;
  }
  LOG(WARNING) << "Called MorePayloadIncoming() on an empty ResponseApdu";
  return false;
}

void ResponseApdu::RemoveStatusBytes() {
  if (2 <= data_.size()) {
    data_.pop_back();
    data_.pop_back();
  }
}

}  // namespace hermes
