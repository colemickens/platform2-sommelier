// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_SMS_MESSAGE_H_
#define CROMO_SMS_MESSAGE_H_

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <string>
#include <vector>

// Simple class that represents SMS message fragments and their
// metadata.
class SmsMessageFragment {
 public:
  // Create an SMS message fragment from a PDU (Protocol Description
  // Unit) as documented in 3GPP 23.040. Text of the message is
  // represented in the GSM 7 alphabet as documented in 3GPP 23.038,
  static SmsMessageFragment* CreateFragment(const uint8_t* pdu, size_t pdu_len,
                                            int index);

  std::string& smsc_address() { return smsc_address_; }
  std::string& sender_address() { return sender_address_; }
  std::string& timestamp() { return timestamp_; }
  // Return the body of the SMS message as a UTF-8 encoded string
  virtual std::string& text() { return text_; }
  int part_reference() { return part_reference_; }
  int part_sequence() { return part_sequence_; }
  int part_count() { return part_count_; }

  int index() { return index_; }

 protected:
  SmsMessageFragment(std::string& smsc_address,
                     std::string& sender_address,
                     std::string& timestamp,
                     std::string& text,
                     int part_reference,
                     int part_sequence,
                     int part_count,
                     int index) :
    smsc_address_(smsc_address),
    sender_address_(sender_address),
    timestamp_(timestamp),
    text_(text),
    part_reference_(part_reference),
    part_sequence_(part_sequence),
    part_count_(part_count),
    index_(index) {}
 private:
  SmsMessageFragment() {}       // disallow no-arg constructor invocation


  std::string smsc_address_;
  std::string sender_address_;
  std::string timestamp_;
  std::string text_;

  int part_reference_;
  int part_sequence_;
  int part_count_;

  int index_;

  DISALLOW_COPY_AND_ASSIGN(SmsMessageFragment);
};

// Class that represents a full SMS message composed of one or more
// message fragments. This does the work of tracking whether all the
// fragments are present and concatenating the message text.
class SmsMessage {
public:
  std::string& smsc_address() { return base_->smsc_address(); }
  std::string& sender_address() { return base_->sender_address(); }
  std::string& timestamp() { return base_->timestamp(); }
  int index() { return base_->index(); }
  int part_reference() { return base_->part_reference(); }
  int part_count() { return base_->part_count(); }

  std::string& text();
  bool is_complete();
  std::vector<int> index_list();

  SmsMessage(SmsMessageFragment *base);
  void add(SmsMessageFragment *sms);
  ~SmsMessage();

private:
  SmsMessageFragment* base_;
  int num_remaining_parts_;
  std::vector<SmsMessageFragment *> fragments_;
  std::string composite_text_;

  DISALLOW_COPY_AND_ASSIGN(SmsMessage);
};

#endif // CROMO_SMS_MESSAGE_H_
