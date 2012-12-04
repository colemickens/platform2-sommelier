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

#include "shill/nl80211_attribute.h"

struct nlattr;
struct nlmsghdr;

namespace shill {

// Class for messages received from libnl.
class UserBoundNlMessage {
 public:
  static const unsigned int kEthernetAddressBytes;

  // A const iterator to the attribute names in the attributes_ map of a
  // UserBoundNlMessage.  The purpose, here, is to hide the way that the
  // attribute is stored.
  class AttributeNameIterator {
   public:
    explicit AttributeNameIterator(const std::map<nl80211_attrs,
                                                  nlattr *> &map_param)
        : map_(map_param) {
      iter_ = map_.begin();
    }

    // Causes the iterator to point to the next attribute in the list.
    void Advance() { ++iter_; }

    // Returns 'true' if the iterator points beyond the last attribute in the
    // list; returns 'false' otherwise.
    bool AtEnd() const { return iter_ == map_.end(); }

    // Returns the attribute name (which is actually an 'enum' value).
    nl80211_attrs GetName() const { return iter_->first; }

   private:
    const std::map<nl80211_attrs, nlattr *> &map_;
    std::map<nl80211_attrs, nlattr *>::const_iterator iter_;

    DISALLOW_COPY_AND_ASSIGN(AttributeNameIterator);
  };

  static const char kBogusMacAddress[];

  UserBoundNlMessage(uint8 message_type, const char *message_type_string)
      : message_(NULL),
        message_type_(message_type),
        message_type_string_(message_type_string) { }
  virtual ~UserBoundNlMessage();

  // Non-trivial initialization.
  virtual bool Init(nlattr *tb[NL80211_ATTR_MAX + 1], nlmsghdr *msg);

  // Provide a suite of methods to allow (const) iteration over the names of the
  // attributes inside a message object.

  AttributeNameIterator *GetAttributeNameIterator() const;

  // Other ways to see the internals of the object.

  // Return true if the attribute is in our map, regardless of the value of
  // the attribute, itself.
  bool AttributeExists(nl80211_attrs name) const;

  // Message ID is equivalent to the message's sequence number.
  uint32_t GetId() const;

  // Returns the data type of a given attribute.
  Nl80211Attribute::Type GetAttributeType(nl80211_attrs name) const;

  // Returns a string describing the data type of a given attribute.
  std::string GetAttributeTypeString(nl80211_attrs name) const;

  // If successful, returns 'true' and the raw attribute data (after the
  // header) into |value| for this attribute.  If no attribute by this name
  // exists in this message, clears |value| and returns 'false'.  If otherwise
  // unsuccessful, returns 'false' and leaves |value| unchanged.
  bool GetRawAttributeData(nl80211_attrs name, ByteString *value) const;

  // Each of these methods set |value| with the value of the specified
  // attribute (if the attribute is not found, |value| remains unchanged).

  bool GetStringAttribute(nl80211_attrs name, std::string *value) const;
  bool GetU8Attribute(nl80211_attrs name, uint8_t *value) const;
  bool GetU16Attribute(nl80211_attrs name, uint16_t *value) const;
  bool GetU32Attribute(nl80211_attrs name, uint32_t *value) const;
  bool GetU64Attribute(nl80211_attrs name, uint64_t *value) const;

  // Fill a string with characters that represents the value of the attribute.
  // If no attribute is found or if the type isn't trivially stringizable,
  // this method returns 'false' and |value| remains unchanged.
  bool GetAttributeString(nl80211_attrs name, std::string *value) const;

  // Helper function to provide a string for a MAC address.  If no attribute
  // is found, this method returns 'false'.  On any error with a non-NULL
  // |value|, this method sets |value| to a bogus MAC address.
  bool GetMacAttributeString(nl80211_attrs name, std::string *value) const;

  // Helper function to provide a vector of scan frequencies for attributes
  // that contain them (such as NL80211_ATTR_SCAN_FREQUENCIES).
  bool GetScanFrequenciesAttribute(enum nl80211_attrs name,
                                   std::vector<uint32_t> *value) const;

  // Helper function to provide a vector of SSIDs for attributes that contain
  // them (such as NL80211_ATTR_SCAN_SSIDS).
  bool GetScanSsidsAttribute(enum nl80211_attrs name,
                             std::vector<std::string> *value) const;

  // Writes the raw attribute data to a string.  For debug.
  virtual std::string RawToString(nl80211_attrs name) const;

  // Returns a string describing a given attribute name.
  static std::string StringFromAttributeName(nl80211_attrs name);

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
  // Duplicate attribute data, store in map indexed on 'name'.
  bool AddAttribute(nl80211_attrs name, nlattr *data);

  // Returns the raw nlattr for a given attribute (NULL if attribute doesn't
  // exist).
  const nlattr *GetAttribute(nl80211_attrs name) const;

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
  friend class AttributeNameIterator;
  friend class Config80211Test;
  FRIEND_TEST(Config80211Test, NL80211_CMD_NOTIFY_CQM);

  static const uint32_t kIllegalMessage;

  nlmsghdr *message_;
  const uint8 message_type_;
  const char *message_type_string_;
  static std::map<uint16_t, std::string> *reason_code_string_;
  static std::map<uint16_t, std::string> *status_code_string_;
  std::map<nl80211_attrs, nlattr *> attributes_;

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

  Nl80211Frame(const ByteString &init);
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
