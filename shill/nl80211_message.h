// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_USER_BOUND_NLMESSAGE_H_
#define SHILL_USER_BOUND_NLMESSAGE_H_

#include <linux/nl80211.h>

#include <iomanip>
#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/bind.h>
#include <base/lazy_instance.h>
#include <gtest/gtest.h>

#include "shill/attribute_list.h"
#include "shill/byte_string.h"
#include "shill/refptr_types.h"

struct nlattr;
struct nlmsghdr;

namespace shill {

// Class for messages received from libnl.
class Nl80211Message {
 public:
  static const unsigned int kEthernetAddressBytes;
  static const char kBogusMacAddress[];

  Nl80211Message(uint8 message_type, const char *message_type_string)
      : message_type_(message_type),
        message_type_string_(message_type_string),
        sequence_number_(kIllegalMessage),
        attributes_(new AttributeList) {}
  virtual ~Nl80211Message() {}

  // Initializes the message with bytes from the kernel.
  virtual bool InitFromNlmsg(const nlmsghdr *msg);
  static void PrintBytes(int log_level, const unsigned char *buf,
                         size_t num_bytes);
  uint32_t sequence_number() const { return sequence_number_; }
  virtual void Print(int log_level) const;

  void set_sequence_number(uint32_t seq) { sequence_number_ = seq; }

  AttributeListConstRefPtr const_attributes() const { return attributes_; }
  AttributeListRefPtr attributes() { return attributes_; }

  // TODO(wdg): This needs to be moved to AttributeMac.
  // Helper function to provide a string for a MAC address.  If no attribute
  // is found, this method returns 'false'.  On any error with a non-NULL
  // |value|, this method sets |value| to a bogus MAC address.
  bool GetMacAttributeString(int id, std::string *value) const;

  // TODO(wdg): This needs to be moved to AttributeScanFrequencies.
  // Helper function to provide a vector of scan frequencies for attributes
  // that contain them (such as NL80211_ATTR_SCAN_FREQUENCIES).
  bool GetScanFrequenciesAttribute(int id, std::vector<uint32_t> *value) const;

  // TODO(wdg): This needs to be moved to AttributeScanSSids.
  // Helper function to provide a vector of SSIDs for attributes that contain
  // them (such as NL80211_ATTR_SCAN_SSIDS).
  bool GetScanSsidsAttribute(int id, std::vector<std::string> *value) const;

  // TODO(wdg): This needs to be moved to AttributeMac.
  // Stringizes the MAC address found in 'arg'.  If there are problems (such
  // as a NULL |arg|), |value| is set to a bogus MAC address.
  static std::string StringFromMacAddress(const uint8_t *arg);

  // Returns a string representing the passed-in |status| or |reason|, the
  // value of which has been acquired from libnl (for example, from the
  // NL80211_ATTR_STATUS_CODE or NL80211_ATTR_REASON_CODE attribute).
  static std::string StringFromReason(uint16_t reason);
  static std::string StringFromStatus(uint16_t status);

  uint8 message_type() const { return message_type_; }
  const char *message_type_string() const { return message_type_string_; }

  // Returns a netlink message suitable for Sockets::Send.  Return value is
  // empty on failure.  |nlmsg_type| needs to be the family id returned by
  // |genl_ctrl_resolve|.
  ByteString Encode(uint16_t nlmsg_type) const;

 protected:
  // Returns a string that should precede all user-bound message strings.
  virtual std::string GetHeaderString() const;

  // Returns a string that describes the contents of the frame pointed to by
  // 'attr'.
  std::string StringFromFrame(int attr_name) const;

  // Converts key_type to a string.
  static std::string StringFromKeyType(nl80211_key_type key_type);

  // Returns a string representation of the REG initiator described by the
  // method's parameter.
  static std::string StringFromRegInitiator(__u8 initiator);

  // Returns a string based on the SSID found in 'data'.  Non-printable
  // characters are string-ized.
  static std::string StringFromSsid(const uint8_t len, const uint8_t *data);

 private:
  friend class Config80211Test;
  FRIEND_TEST(Config80211Test, NL80211_CMD_NOTIFY_CQM);

  static const uint32_t kIllegalMessage;

  const uint8 message_type_;
  const char *message_type_string_;
  static std::map<uint16_t, std::string> *reason_code_string_;
  static std::map<uint16_t, std::string> *status_code_string_;
  uint32_t sequence_number_;

  AttributeListRefPtr attributes_;

  DISALLOW_COPY_AND_ASSIGN(Nl80211Message);
};

class Nl80211Frame {
 public:
  enum Type {
    kAssocResponseFrameType = 0x10,
    kReassocResponseFrameType = 0x30,
    kAssocRequestFrameType = 0x00,
    kReassocRequestFrameType = 0x20,
    kAuthFrameType = 0xb0,
    kDisassocFrameType = 0xa0,
    kDeauthFrameType = 0xc0,
    kIllegalFrameType = 0xff
  };

  explicit Nl80211Frame(const ByteString &init);
  bool ToString(std::string *output) const;
  bool IsEqual(const Nl80211Frame &other) const;
  uint16_t reason() const { return reason_; }
  uint16_t status() const { return status_; }

 private:
  static const uint8_t kMinimumFrameByteCount;
  static const uint8_t kFrameTypeMask;

  std::string mac_from_;
  std::string mac_to_;
  uint8_t frame_type_;
  uint16_t reason_;
  uint16_t status_;
  ByteString frame_;

  DISALLOW_COPY_AND_ASSIGN(Nl80211Frame);
};

//
// Specific Nl80211Message types.
//

class AckMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  AckMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AckMessage);
};

class AssociateMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  AssociateMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AssociateMessage);
};


class AuthenticateMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  AuthenticateMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticateMessage);
};


class CancelRemainOnChannelMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  CancelRemainOnChannelMessage()
      : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelRemainOnChannelMessage);
};


class ConnectMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  ConnectMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectMessage);
};


class DeauthenticateMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DeauthenticateMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DeauthenticateMessage);
};


class DeleteStationMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DeleteStationMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteStationMessage);
};


class DisassociateMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DisassociateMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisassociateMessage);
};


class DisconnectMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DisconnectMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DisconnectMessage);
};


class ErrorMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  explicit ErrorMessage(uint32_t error);
  virtual std::string ToString() const;

 private:
  uint32_t error_;

  DISALLOW_COPY_AND_ASSIGN(ErrorMessage);
};


class FrameTxStatusMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  FrameTxStatusMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameTxStatusMessage);
};

class GetRegMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  GetRegMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GetRegMessage);
};


class JoinIbssMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  JoinIbssMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(JoinIbssMessage);
};


class MichaelMicFailureMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  MichaelMicFailureMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MichaelMicFailureMessage);
};


class NewScanResultsMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewScanResultsMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewScanResultsMessage);
};


class NewStationMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewStationMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewStationMessage);
};


class NewWifiMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewWifiMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWifiMessage);
};


class NoopMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NoopMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopMessage);
};


class NotifyCqmMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NotifyCqmMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifyCqmMessage);
};


class PmksaCandidateMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  PmksaCandidateMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PmksaCandidateMessage);
};


class RegBeaconHintMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RegBeaconHintMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RegBeaconHintMessage);
};


class RegChangeMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RegChangeMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RegChangeMessage);
};


class RemainOnChannelMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RemainOnChannelMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RemainOnChannelMessage);
};


class RoamMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RoamMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RoamMessage);
};


class ScanAbortedMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  ScanAbortedMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ScanAbortedMessage);
};


class TriggerScanMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  TriggerScanMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TriggerScanMessage);
};


class UnknownMessage : public Nl80211Message {
 public:
  explicit UnknownMessage(uint8_t command)
      : Nl80211Message(command, kCommandString),
        command_(command) {}

  static const uint8_t kCommand;
  static const char kCommandString[];

 private:
  uint8_t command_;

  DISALLOW_COPY_AND_ASSIGN(UnknownMessage);
};


class UnprotDeauthenticateMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  UnprotDeauthenticateMessage()
      : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UnprotDeauthenticateMessage);
};


class UnprotDisassociateMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  UnprotDisassociateMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(UnprotDisassociateMessage);
};


//
// Factory class.
//

class Nl80211MessageFactory {
 public:
  // Ownership of the message is passed to the caller and, as such, he should
  // delete it.
  static Nl80211Message *CreateMessage(nlmsghdr *msg);

 private:
  DISALLOW_COPY_AND_ASSIGN(Nl80211MessageFactory);
};


// Nl80211MessageDataCollector - this class is used to collect data to be
// used for unit tests.  It is only invoked in this case.

class Nl80211MessageDataCollector {
 public:
  static Nl80211MessageDataCollector *GetInstance();

  void CollectDebugData(const Nl80211Message &message, nlmsghdr *msg);

 protected:
  friend struct
      base::DefaultLazyInstanceTraits<Nl80211MessageDataCollector>;

  explicit Nl80211MessageDataCollector();

 private:
  // In order to limit the output from this class, I keep track of types I
  // haven't yet printed.
  std::map<uint8_t, bool> need_to_print;

  DISALLOW_COPY_AND_ASSIGN(Nl80211MessageDataCollector);
};

}  // namespace shill

#endif  // SHILL_USER_BOUND_NLMESSAGE_H_
