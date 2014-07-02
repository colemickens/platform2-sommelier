// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_GENERIC_NETLINK_MESSAGE_H_
#define SHILL_GENERIC_NETLINK_MESSAGE_H_

#include "shill/attribute_list.h"
#include "shill/byte_string.h"
#include "shill/netlink_message.h"
#include "shill/refptr_types.h"

struct nlmsghdr;

namespace shill {

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

  virtual void Print(int header_log_level, int detail_log_level) const;

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

  static uint16_t GetMessageType() { return kMessageType; }

  virtual bool InitFromNlmsg(const nlmsghdr *msg);

  // Message factory for all types of Control netlink message.
  static NetlinkMessage *CreateMessage(const nlmsghdr *const_msg);

 private:
  DISALLOW_COPY_AND_ASSIGN(ControlNetlinkMessage);
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

  GetFamilyMessage();

 private:
  DISALLOW_COPY_AND_ASSIGN(GetFamilyMessage);
};

class UnknownControlMessage : public ControlNetlinkMessage {
 public:
  explicit UnknownControlMessage(uint8_t command)
      : ControlNetlinkMessage(command, "<UNKNOWN CONTROL MESSAGE>"),
        command_(command) {}

 private:
  uint8_t command_;
  DISALLOW_COPY_AND_ASSIGN(UnknownControlMessage);
};

}  // namespace shill

#endif  // SHILL_GENERIC_NETLINK_MESSAGE_H_
