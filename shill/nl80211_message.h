// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_NL80211_MESSAGE_H_
#define SHILL_NL80211_MESSAGE_H_

#include <map>
#include <string>
#include <vector>

#include <base/lazy_instance.h>

#include "shill/byte_string.h"
#include "shill/generic_netlink_message.h"

struct nlmsghdr;

namespace shill {

// Class for messages received from the mac80211 drivers by way of the
// cfg80211 kernel module.
class Nl80211Message : public GenericNetlinkMessage {
 public:
  static const char kMessageTypeString[];

  Nl80211Message(uint8_t command, const char *command_string)
      : GenericNetlinkMessage(nl80211_message_type_, command, command_string) {}
  virtual ~Nl80211Message() {}

  // Gets the family_id / message_type for all Nl80211 messages.
  static uint16_t GetMessageType();

  // Sets the family_id / message_type for all Nl80211 messages.
  static void SetMessageType(uint16_t message_type);

  virtual bool InitFromNlmsg(const nlmsghdr *msg);

  uint8_t command() const { return command_; }
  const char *command_string() const { return command_string_; }
  uint16_t message_type() const { return message_type_; }
  uint32_t sequence_number() const { return sequence_number_; }
  void set_sequence_number(uint32_t seq) { sequence_number_ = seq; }

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

class GetStationMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  GetStationMessage();

 private:
  DISALLOW_COPY_AND_ASSIGN(GetStationMessage);
};

class SetWakeOnPacketConnMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  SetWakeOnPacketConnMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SetWakeOnPacketConnMessage);
};

class GetWiphyMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  GetWiphyMessage();

 private:
  DISALLOW_COPY_AND_ASSIGN(GetWiphyMessage);
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


class NewWiphyMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewWiphyMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWiphyMessage);
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

  TriggerScanMessage();

 private:
  DISALLOW_COPY_AND_ASSIGN(TriggerScanMessage);
};


class UnknownNl80211Message : public Nl80211Message {
 public:
  explicit UnknownNl80211Message(uint8_t command)
      : Nl80211Message(command, "<UNKNOWN NL80211 MESSAGE>"),
        command_(command) {}

 private:
  uint8_t command_;
  DISALLOW_COPY_AND_ASSIGN(UnknownNl80211Message);
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


class GetInterfaceMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  GetInterfaceMessage();

 private:
  DISALLOW_COPY_AND_ASSIGN(GetInterfaceMessage);
};

class NewInterfaceMessage : public Nl80211Message {
 public:
  static const uint8_t kCommand;
  static const char kCommandString[];

  NewInterfaceMessage() : Nl80211Message(kCommand, kCommandString) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewInterfaceMessage);
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

  Nl80211MessageDataCollector();

 private:
  // In order to limit the output from this class, I keep track of types I
  // haven't yet printed.
  std::map<uint8_t, bool> need_to_print;

  DISALLOW_COPY_AND_ASSIGN(Nl80211MessageDataCollector);
};

}  // namespace shill

#endif  // SHILL_NL80211_MESSAGE_H_
