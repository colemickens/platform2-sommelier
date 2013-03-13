// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NL80211_MESSAGE_H_
#define SHILL_NL80211_MESSAGE_H_

#include <map>
#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/lazy_instance.h>

#include <gtest/gtest.h>  // for FRIEND_TEST.

#include "shill/attribute_list.h"
#include "shill/byte_string.h"
#include "shill/refptr_types.h"

struct nlmsghdr;

namespace shill {

class Config80211;

// Netlink messages are sent over netlink sockets to talk between user-space
// programs (like shill) and kernel modules (like the cfg80211 module).  Each
// kernel module that talks netlink potentially adds its own family header to
// the nlmsghdr (the top-level netlink message header) and, potentially, uses a
// different payload format.  The NetlinkMessage class represents that which
// is common between the different types of netlink message.
//
// The common portions of Netlink Messages start with a |nlmsghdr|.  Those
// messages look something like the following (the functions, macros, and
// datatypes described are provided by libnl -- see also
// http://www.infradead.org/~tgr/libnl/doc/core.html):
//
//         |<--------------nlmsg_total_size()----------->|
//         |       |<------nlmsg_datalen()-------------->|
//         |       |                                     |
//    -----+-----+-+-----------------------------------+-+----
//     ... |     | |            netlink payload        | |
//         |     | +------------+-+--------------------+ |
//         | nl  | |            | |                    | | nl
//         | msg |p| (optional) |p|                    |p| msg ...
//         | hdr |a| family     |a|   family payload   |a| hdr
//         |     |d| header     |d|                    |d|
//         |     | |            | |                    | |
//    -----+-----+-+------------+-+--------------------+-+----
//                  ^
//                  |
//                  +-- nlmsg_data()
//
// All NetlinkMessages sent to the kernel need a valid message type (which is
// found in the nlmsghdr structure) and all NetlinkMessages received from the
// kernel have a valid message type.  Some message types (NLMSG_NOOP,
// NLMSG_ERROR, and GENL_ID_CTRL, for example) are allocated statically; for
// those, the |message_type_| is assigned directly.
//
// Other message types ("nl80211", for example), are assigned by the kernel
// dynamically.  To get the message type, pass a closure to assign the
// message_type along with the sting to Config80211::GetFamily:
//
//  nl80211_type = config80211_->GetFamily(Nl80211Message::kMessageType);
//
// Do all of this before you start to create NetlinkMessages so that
// NetlinkMessage can be instantiated with a valid |message_type_|.

class NetlinkMessage {
 public:
  static const uint32_t kBroadcastSequenceNumber;
  static const uint16_t kIllegalMessageType;

  explicit NetlinkMessage(uint16_t message_type) :
      flags_(0), message_type_(message_type),
      sequence_number_(kBroadcastSequenceNumber) {}
  virtual ~NetlinkMessage() {}

  // Returns a string of bytes representing the message (with it headers) and
  // any necessary padding.  These bytes are appropriately formatted to be
  // written to a netlink socket.
  virtual ByteString Encode(uint32_t sequence_number) = 0;

  // Initializes the |NetlinkMessage| from a complete and legal message
  // (potentially received from the kernel via a netlink socket).
  virtual bool InitFromNlmsg(const nlmsghdr *msg);

  uint16_t message_type() const { return message_type_; }
  void AddFlag(uint16_t new_flag) { flags_ |= new_flag; }
  uint16_t flags() const { return flags_; }
  uint32_t sequence_number() const { return sequence_number_; }
  virtual void Print(int log_level) const = 0;

  // Logs the message's raw bytes (with minimal interpretation).
  static void PrintBytes(int log_level, const unsigned char *buf,
                         size_t num_bytes);

 protected:
  friend class Config80211Test;
  FRIEND_TEST(Config80211Test, NL80211_CMD_NOTIFY_CQM);

  // Returns a string of bytes representing an |nlmsghdr|, filled-in, and its
  // padding.
  virtual ByteString EncodeHeader(uint32_t sequence_number);
  // Reads the |nlmsghdr| and removes it from |input|.
  virtual bool InitAndStripHeader(ByteString *input);

  uint16_t flags_;
  uint16_t message_type_;
  uint32_t sequence_number_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetlinkMessage);
};


// The Error and Ack messages are received from the kernel and are combined,
// here, because they look so much alike (the only difference is that the
// error code is 0 for the Ack messages).  Error messages are received from
// the kernel in response to a sent message when there's a problem (such as
// a malformed message or a busy kernel module).  Ack messages are received
// from the kernel when a sent message has the NLM_F_ACK flag set, indicating
// that an Ack is requested.
class ErrorAckMessage : public NetlinkMessage {
 public:
  static const uint16_t kMessageType;

  ErrorAckMessage() : NetlinkMessage(kMessageType), error_(0) {}
  virtual bool InitFromNlmsg(const nlmsghdr *const_msg);
  virtual ByteString Encode(uint32_t sequence_number);
  virtual void Print(int log_level) const;
  std::string ToString() const;
  uint32_t error() const { return -error_; }

 private:
  uint32_t error_;

  DISALLOW_COPY_AND_ASSIGN(ErrorAckMessage);
};


class NoopMessage : public NetlinkMessage {
 public:
  static const uint16_t kMessageType;

  NoopMessage() : NetlinkMessage(kMessageType) {}
  virtual ByteString Encode(uint32_t sequence_number);
  virtual void Print(int log_level) const;
  std::string ToString() const { return "<NOOP>"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopMessage);
};


class DoneMessage : public NetlinkMessage {
 public:
  static const uint16_t kMessageType;

  DoneMessage() : NetlinkMessage(kMessageType) {}
  virtual ByteString Encode(uint32_t sequence_number);
  virtual void Print(int log_level) const;
  std::string ToString() const { return "<DONE with multipart message>"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(DoneMessage);
};


class OverrunMessage : public NetlinkMessage {
 public:
  static const uint16_t kMessageType;

  OverrunMessage() : NetlinkMessage(kMessageType) {}
  virtual ByteString Encode(uint32_t sequence_number);
  virtual void Print(int log_level) const;
  std::string ToString() const { return "<OVERRUN - data lost>"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverrunMessage);
};


class UnknownMessage : public NetlinkMessage {
 public:
  UnknownMessage(uint16_t message_type, ByteString message_body) :
      NetlinkMessage(message_type), message_body_(message_body) {}
  virtual ByteString Encode(uint32_t sequence_number);
  virtual void Print(int log_level) const;

 private:
  ByteString message_body_;

  DISALLOW_COPY_AND_ASSIGN(UnknownMessage);
};

// Objects of the |GenericNetlinkMessage| type represent messages that contain
// a |genlmsghdr| after a |nlmsghdr|.  These messages seem to all contain a
// payload that consists of a list of structured attributes (it's possible that
// some messages might have a genlmsghdr and a different kind of payload but I
// haven't seen one, yet).  The genlmsghdr contains a command id that, when
// combined with the family_id (from the nlmsghdr), describes the ultimate use
// for the netlink message.
//
// An attribute contains a header and a chunk of data. The header contains an
// id which is an enumerated value that describes the use of the attribute's
// data (the datatype of the attribute's data is implied by the attribute id)
// and the length of the header+data in bytes.  The attribute id is,
// confusingly, called the type (or nla_type -- this is _not_ the data type of
// the attribute).  Each family defines the meaning of the nla_types in the
// context of messages in that family (for example, the nla_type with the
// value 3 will always mean the same thing for attributes in the same family).
// EXCEPTION: Some attributes are nested (that is, they contain a list of other
// attributes rather than a single value).  Each nested attribute defines the
// meaning of the nla_types in the context of attributes that are nested under
// this attribute (for example, the nla_type with the value 3 will have a
// different meaning when nested under another attribute -- that meaning is
// defined by the attribute under which it is nested).  Fun.
//
// The GenericNetlink messages look like this:
//
// -----+-----+-+-------------------------------------------------+-+--
//  ... |     | |              message payload                    | |
//      |     | +------+-+----------------------------------------+ |
//      | nl  | |      | |                attributes              | |
//      | msg |p| genl |p+-----------+-+---------+-+--------+-----+p| ...
//      | hdr |a| msg  |a|  struct   |p| attrib  |p| struct | ... |a|
//      |     |d| hdr  |d|  nlattr   |a| payload |a| nlattr |     |d|
//      |     | |      | |           |d|         |d|        |     | |
// -----+-----+-+------+-+-----------+-+---------+-+--------+-----+-+--
//                       |              ^        | |
//                       |<-NLA_HDRLEN->|        | |
//                       |              +---nla_data()
//                       |<----nla_attr_size---->| |
//                       |<-----nla_total_size---->|

class GenericNetlinkMessage : public NetlinkMessage {
 public:
  GenericNetlinkMessage(uint16_t my_message_type, uint8 command,
                        const char *command_string)
      : NetlinkMessage(my_message_type),
        attributes_(new AttributeList),
        command_(command),
        command_string_(command_string) {}
  virtual ~GenericNetlinkMessage() {}

  virtual ByteString Encode(uint32_t sequence_number);

  uint8 command() const { return command_; }
  const char *command_string() const { return command_string_; }
  AttributeListConstRefPtr const_attributes() const { return attributes_; }
  AttributeListRefPtr attributes() { return attributes_; }

  virtual void Print(int log_level) const;

 protected:
  // Returns a string of bytes representing _both_ an |nlmsghdr| and a
  // |genlmsghdr|, filled-in, and its padding.
  virtual ByteString EncodeHeader(uint32_t sequence_number);
  // Reads the |nlmsghdr| and |genlmsghdr| headers and removes them from
  // |input|.
  virtual bool InitAndStripHeader(ByteString *input);

  AttributeListRefPtr attributes_;
  const uint8 command_;
  const char *command_string_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GenericNetlinkMessage);
};

// Control Messages

class ControlNetlinkMessage : public GenericNetlinkMessage {
 public:
  static const uint16_t kMessageType;
  ControlNetlinkMessage(uint8 command, const char *command_string)
      : GenericNetlinkMessage(kMessageType, command, command_string) {}

  virtual bool InitFromNlmsg(const nlmsghdr *msg);

  // Message factory for all types of Control netlink message.
  static NetlinkMessage *CreateMessage(const nlmsghdr *const_msg);
};

class NewFamilyMessage : public ControlNetlinkMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewFamilyMessage() : ControlNetlinkMessage(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewFamilyMessage);
};

class GetFamilyMessage : public ControlNetlinkMessage {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  GetFamilyMessage() : ControlNetlinkMessage(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GetFamilyMessage);
};


// Class for messages received from the mac80211 drivers by way of the
// cfg80211 kernel module.
class Nl80211Message : public GenericNetlinkMessage {
 public:
  static const char kMessageTypeString[];
  static const unsigned int kEthernetAddressBytes;
  static const char kBogusMacAddress[];

  Nl80211Message(uint8 command, const char *command_string)
      : GenericNetlinkMessage(nl80211_message_type_, command, command_string) {}
  virtual ~Nl80211Message() {}

  // Sets the family_id / message_type for all Nl80211 messages.
  static void SetMessageType(uint16_t message_type);

  virtual bool InitFromNlmsg(const nlmsghdr *msg);

  uint8 command() const { return command_; }
  const char *command_string() const { return command_string_; }
  uint16_t message_type() const { return message_type_; }
  uint32_t sequence_number() const { return sequence_number_; }
  void set_sequence_number(uint32_t seq) { sequence_number_ = seq; }

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

  // Message factory for all types of Nl80211 message.
  static NetlinkMessage *CreateMessage(const nlmsghdr *const_msg);

 private:
  static std::map<uint16_t, std::string> *reason_code_string_;
  static std::map<uint16_t, std::string> *status_code_string_;
  static uint16_t nl80211_message_type_;

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


class GetScanMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  GetScanMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(GetScanMessage);
};


class TriggerScanMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  TriggerScanMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TriggerScanMessage);
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

class NetlinkMessageFactory {
 public:
  typedef base::Callback<NetlinkMessage *(const nlmsghdr *msg)> FactoryMethod;

  NetlinkMessageFactory() {}

  // Adds a message factory for a specific message_type.  Intended to be used
  // at initialization.
  bool AddFactoryMethod(uint16_t message_type, FactoryMethod factory);

  // Ownership of the message is passed to the caller and, as such, he should
  // delete it.
  NetlinkMessage *CreateMessage(const nlmsghdr *msg) const;

 private:
  std::map<uint16_t, FactoryMethod> factories_;

  DISALLOW_COPY_AND_ASSIGN(NetlinkMessageFactory);
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

#endif  // SHILL_NL80211_MESSAGE_H_
