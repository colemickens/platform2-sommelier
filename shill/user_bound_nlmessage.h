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

struct nlattr;
struct nlmsghdr;

namespace shill {

// Class for messages received from libnl.
class UserBoundNlMessage {
 public:
  static const unsigned int kEthernetAddressBytes;
  static const char kBogusMacAddress[];

  UserBoundNlMessage(uint8 message_type, const char *message_type_string)
      : message_(NULL),
        message_type_(message_type),
        message_type_string_(message_type_string) {}
  virtual ~UserBoundNlMessage() {}

  // Non-trivial initialization.
  virtual bool Init(nlattr *tb[NL80211_ATTR_MAX + 1], nlmsghdr *msg);

  // Message ID is equivalent to the message's sequence number.
  uint32_t GetId() const;

  const AttributeList &attributes() const { return attributes_; }

  // TODO(wdg): This needs to be moved to AttributeMac.
  // Helper function to provide a string for a MAC address.  If no attribute
  // is found, this method returns 'false'.  On any error with a non-NULL
  // |value|, this method sets |value| to a bogus MAC address.
  bool GetMacAttributeString(nl80211_attrs name, std::string *value) const;

  // TODO(wdg): This needs to be moved to AttributeScanFrequencies.
  // Helper function to provide a vector of scan frequencies for attributes
  // that contain them (such as NL80211_ATTR_SCAN_FREQUENCIES).
  bool GetScanFrequenciesAttribute(enum nl80211_attrs name,
                                   std::vector<uint32_t> *value) const;

  // TODO(wdg): This needs to be moved to AttributeScanSSids.
  // Helper function to provide a vector of SSIDs for attributes that contain
  // them (such as NL80211_ATTR_SCAN_SSIDS).
  bool GetScanSsidsAttribute(enum nl80211_attrs name,
                             std::vector<std::string> *value) const;

  // TODO(wdg): This needs to be moved to AttributeMac.
  // Stringizes the MAC address found in 'arg'.  If there are problems (such
  // as a NULL |arg|), |value| is set to a bogus MAC address.
  static std::string StringFromMacAddress(const uint8_t *arg);

  // Returns a string representing the passed-in |status| or |reason|, the
  // value of which has been acquired from libnl (for example, from the
  // NL80211_ATTR_STATUS_CODE or NL80211_ATTR_REASON_CODE attribute).
  static std::string StringFromReason(uint16_t reason);
  static std::string StringFromStatus(uint16_t status);

  // Returns a string that describes this message.
  virtual std::string ToString() const { return GetHeaderString(); }

  uint8 message_type() const { return message_type_; }
  const char *message_type_string() const { return message_type_string_; }

 protected:
  // Returns a string that should precede all user-bound message strings.
  virtual std::string GetHeaderString() const;

  // Returns a string that describes the contents of the frame pointed to by
  // 'attr'.
  std::string StringFromFrame(nl80211_attrs attr_name) const;

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

  nlmsghdr *message_;
  const uint8 message_type_;
  const char *message_type_string_;
  static std::map<uint16_t, std::string> *reason_code_string_;
  static std::map<uint16_t, std::string> *status_code_string_;

  AttributeList attributes_;

  DISALLOW_COPY_AND_ASSIGN(UserBoundNlMessage);
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
// Specific UserBoundNlMessage types.
//

class AssociateMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  AssociateMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssociateMessage);
};


class AuthenticateMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  AuthenticateMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(AuthenticateMessage);
};


class CancelRemainOnChannelMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  CancelRemainOnChannelMessage()
      : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(CancelRemainOnChannelMessage);
};


class ConnectMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  ConnectMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectMessage);
};


class DeauthenticateMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DeauthenticateMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeauthenticateMessage);
};


class DeleteStationMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DeleteStationMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteStationMessage);
};


class DisassociateMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DisassociateMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisassociateMessage);
};


class DisconnectMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  DisconnectMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(DisconnectMessage);
};


class FrameTxStatusMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  FrameTxStatusMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameTxStatusMessage);
};


class JoinIbssMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  JoinIbssMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(JoinIbssMessage);
};


class MichaelMicFailureMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  MichaelMicFailureMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MichaelMicFailureMessage);
};


class NewScanResultsMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewScanResultsMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewScanResultsMessage);
};


class NewStationMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewStationMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewStationMessage);
};


class NewWifiMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewWifiMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWifiMessage);
};


class NotifyCqmMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NotifyCqmMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotifyCqmMessage);
};


class PmksaCandidateMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  PmksaCandidateMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(PmksaCandidateMessage);
};


class RegBeaconHintMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RegBeaconHintMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  struct ieee80211_beacon_channel {
    __u16 center_freq;
    bool passive_scan;
    bool no_ibss;
  };

  // Returns the channel ID calculated from the 802.11 frequency.
  static int ChannelFromIeee80211Frequency(int freq);

  // Sets values in |chan| based on attributes in |tb|, the array of pointers
  // to netlink attributes, indexed by attribute type.
  int ParseBeaconHintChan(const nlattr *tb,
                          ieee80211_beacon_channel *chan) const;

  DISALLOW_COPY_AND_ASSIGN(RegBeaconHintMessage);
};


class RegChangeMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RegChangeMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegChangeMessage);
};


class RemainOnChannelMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RemainOnChannelMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemainOnChannelMessage);
};


class RoamMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  RoamMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(RoamMessage);
};


class ScanAbortedMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  ScanAbortedMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScanAbortedMessage);
};


class TriggerScanMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  TriggerScanMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(TriggerScanMessage);
};


class UnknownMessage : public UserBoundNlMessage {
 public:
  explicit UnknownMessage(uint8_t command)
      : UserBoundNlMessage(command, kCommandString),
        command_(command) {}

  static const uint8_t kCommand;
  static const char kCommandString[];

  virtual std::string ToString() const;

 private:
  uint8_t command_;

  DISALLOW_COPY_AND_ASSIGN(UnknownMessage);
};


class UnprotDeauthenticateMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  UnprotDeauthenticateMessage()
      : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnprotDeauthenticateMessage);
};


class UnprotDisassociateMessage : public UserBoundNlMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  UnprotDisassociateMessage() : UserBoundNlMessage(kCommand, kCommandString) {}

  virtual std::string ToString() const;

 private:
  DISALLOW_COPY_AND_ASSIGN(UnprotDisassociateMessage);
};


//
// Factory class.
//

class UserBoundNlMessageFactory {
 public:
  // Ownership of the message is passed to the caller and, as such, he should
  // delete it.
  static UserBoundNlMessage *CreateMessage(nlmsghdr *msg);

 private:
  DISALLOW_COPY_AND_ASSIGN(UserBoundNlMessageFactory);
};


// UserBoundNlMessageDataCollector - this class is used to collect data to be
// used for unit tests.  It is only invoked in this case.

class UserBoundNlMessageDataCollector {
 public:
  // This is a singleton -- use Config80211::GetInstance()->Foo()
  static UserBoundNlMessageDataCollector *GetInstance();

  void CollectDebugData(const UserBoundNlMessage &message, nlmsghdr *msg);

 protected:
  friend struct
      base::DefaultLazyInstanceTraits<UserBoundNlMessageDataCollector>;

  explicit UserBoundNlMessageDataCollector();

 private:
  // In order to limit the output from this class, I keep track of types I
  // haven't yet printed.
  std::map<uint8_t, bool> need_to_print;
};

}  // namespace shill

#endif  // SHILL_USER_BOUND_NLMESSAGE_H_
