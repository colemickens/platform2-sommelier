// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_SMS_CACHE_H_
#define CROMO_SMS_CACHE_H_

#include <map>
#include <vector>

#include <dbus-c++/types.h>  // for DBus::Error &

#include "utilities.h"
#include "sms_message.h"

// Low-level routines that the caller needs to implement
class SmsModemOperations {
 public:
  virtual SmsMessageFragment* GetSms(int index, DBus::Error& error) = 0;
  virtual void DeleteSms(int index, DBus::Error& error) = 0;
  virtual std::vector<int> ListSms(DBus::Error& error) = 0;
};

// Cache of SMS messages and their index numbers in storage which
// assists in assembling multipart messages.

// Multipart messages are made out of several individual messages with
// the same reference number and part count. The multipart message as
// a whole is referred to by one index number, the canonical index
// number, which is generally the index number of the first part of
// the message seen by the cache. Most operations that take index
// numbers only take canonical index numbers and do not operate on
// bare message fragments.
class SmsCache {
 public:
  explicit SmsCache() {};

  SmsMessage* SmsReceived(int index,
                          DBus::Error& error,
                          SmsModemOperations* impl);

  utilities::DBusPropertyMap Get(int index,
                                 DBus::Error& error,
                                 SmsModemOperations* impl);

  void Delete(int index,
              DBus::Error& error,
              SmsModemOperations* impl);

  std::vector<utilities::DBusPropertyMap> List(DBus::Error& error,
                                               SmsModemOperations* impl);
  ~SmsCache();

 private:

  // Adds the message fragment to the cache, taking ownership of the
  // fragment.
  void put(SmsMessageFragment *message);

  // Get the message corresponding to the index number from the cache,
  // or NULL if there is no such message.
  // If the index refers to the canonical index of a multipart
  // message, the multipart message is returned rather than the
  // original fragment. If the index refers to a non-canonical index
  // of a multipart message, NULL is returned.
  SmsMessage* get(int index);

  // Take the index number of a message fragment and return the
  // canonical index number of the message that fragment belongs to.
  // Returns -1 if no such fragment exists.
  int canonical(int index);

  // Remove and free the message with the corresponding canonical index.
  void remove(int index);

  // Empty the entire cache.
  void clear();

  // Messages by canonical index.
  // Owns messages and hence their fragments.
  std::map<int, SmsMessage *> messages_;

  // Mapping from fragment index to canonical index.
  std::map<int, int> fragments_;

  // Mapping from multipart reference numbers to canonical index
  // of corresponding messages.
  std::map<uint16_t, int> multiparts_;

  DISALLOW_COPY_AND_ASSIGN(SmsCache);

};

#endif // CROMO_SMS_CACHE_H_
