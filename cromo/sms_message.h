// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_SMS_MESSAGE_H_
#define CROMO_SMS_MESSAGE_H_

#include <base/basictypes.h>
#include <string>

// Simple class that represents SMS messages and their
// metadata.
class SmsMessage {
 public:
  // Create an SMS message from a PDU (Protocol Description Unit)
  // as documented in 3GPP 23.040. Text of the message is represented
  // in the GSM 7 alphabet as documented in 3GPP 23.038,
  static SmsMessage* CreateMessage(const uint8_t* pdu, size_t pdu_len);

  std::string& smsc_address() { return smsc_address_; }
  std::string& sender_address() { return sender_address_; }
  std::string& timestamp() { return timestamp_; }
  // Return the body of the SMS message as a UTF-8 encoded string
  std::string& text() { return text_; }

 private:
  SmsMessage() {}       // disallow no-arg constructor invocation

  SmsMessage(std::string& smsc_address,
             std::string& sender_address,
             std::string& timestamp,
             std::string& text) :
    smsc_address_(smsc_address),
    sender_address_(sender_address),
    timestamp_(timestamp),
    text_(text) {}

  std::string smsc_address_;
  std::string sender_address_;
  std::string timestamp_;
  std::string text_;

  DISALLOW_COPY_AND_ASSIGN(SmsMessage);
};

#endif // CROMO_SMS_MESSAGE_H_
