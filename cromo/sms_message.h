// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_SMS_MESSAGE_H_
#define CROMO_SMS_MESSAGE_H_

#include <base/basictypes.h>
#include <string>
#include <vector>

namespace cromo {

// Simple class that represents SMS message fragments and their
// metadata.
class SmsMessageFragment {
 public:
  // Create an SMS message fragment from a PDU (Protocol Description
  // Unit) as documented in 3GPP 23.040.
  static SmsMessageFragment* CreateFragment(const uint8_t* pdu, size_t pdu_len,
                                            int index);

  const std::string& smsc_address() const { return smsc_address_; }
  const std::string& sender_address() const { return sender_address_; }
  const std::string& timestamp() const { return timestamp_; }
  // Return the body of the SMS message as a UTF-8 encoded string
  const std::string& text() const { return text_; }
  int part_reference() const { return part_reference_; }
  int part_sequence() const { return part_sequence_; }
  int part_count() const { return part_count_; }
  int index() const { return index_; }

 protected:
  SmsMessageFragment(const std::string& smsc_address,
                     const std::string& sender_address,
                     const std::string& timestamp,
                     const std::string& text,
                     int part_reference,
                     int part_sequence,
                     int part_count,
                     int index)
    : smsc_address_(smsc_address),
      sender_address_(sender_address),
      timestamp_(timestamp),
      text_(text),
      part_reference_(part_reference),
      part_sequence_(part_sequence),
      part_count_(part_count),
      index_(index) {
  }

 private:
  SmsMessageFragment() {}       // disallow no-arg constructor invocation

  // Address of the carrier's "SMS Center" that sent this fragment.
  const std::string smsc_address_;
  // Address of the message sender.
  const std::string sender_address_;
  // Time the message was sent, including timezone.
  const std::string timestamp_;
  // Contents of the message fragment.
  const std::string text_;

  // An identifier chosen by the SMSC to identify message fragments
  // that are part of the same message.
  int part_reference_;
  // Position of this fragment in the complete message (1 to part_count_).
  int part_sequence_;
  // Number of fragments in this message (1 to 255).
  int part_count_;

  // Storage location of the fragment on the device.
  int index_;

  DISALLOW_COPY_AND_ASSIGN(SmsMessageFragment);
};

// Class that represents a full SMS message composed of one or more
// message fragments. This does the work of tracking whether all the
// fragments are present and concatenating the message text.
class SmsMessage {
 public:
  explicit SmsMessage(SmsMessageFragment* base);
  ~SmsMessage();

  void AddFragment(SmsMessageFragment* sms);

  const std::string& GetMessageText();
  bool IsComplete() const;
  std::vector<int>* MessageIndexList() const;

  const std::string& smsc_address() const { return base_->smsc_address(); }
  const std::string& sender_address() const { return base_->sender_address(); }
  const std::string& timestamp() const { return base_->timestamp(); }
  int index() const { return base_->index(); }
  int part_reference() const { return base_->part_reference(); }
  int part_count() const { return base_->part_count(); }

 private:
  // SMS fragment that stores the non-concatenated parts of the message,
  // including the SMSC address, sender address, and timestamp.
  SmsMessageFragment* base_;
  // Number of parts remaining before the message is fully
  // assembled. Not directly derivable from the size of the fragments_
  // vector because that vector may be sparse.
  int num_remaining_parts_;
  // Storage for the fragments that make up the message, indexed by
  // their part_sequence_ value minus one. Fragments that have not
  // arrived yet are represented as NULL.
  std::vector<SmsMessageFragment*> fragments_;
  // The text of the complete message.
  std::string composite_text_;

  DISALLOW_COPY_AND_ASSIGN(SmsMessage);
};

}  // namespace cromo

#endif  // CROMO_SMS_MESSAGE_H_
