// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sms_cache.h"

#include <glog/logging.h>

#include <mm/mm-modem.h> // for MM_ERROR_MODEM_GSM_INVALIDINDEX


void SmsCache::put(SmsMessageFragment *fragment)
{
  std::map<int, int>::iterator it;
  SmsMessage *message;
  int index = fragment->index();

  it = fragments_.find(index);
  if (it != fragments_.end()) {
    messages_.erase(it->second);
    fragments_.erase(index);
    // TODO(njw): Need to check for and remove from multipart object
    // as well (though this whole clause is kind of a big
    // shouldn't-happen)
  }

  if (fragment->part_count() == 1) {
    message = new SmsMessage(fragment);
    messages_[index] = message;
  } else {
    std::map<uint16_t, int>::iterator multi_it;
    multi_it = multiparts_.find(fragment->part_reference());
    if (multi_it == multiparts_.end()) {
      message = new SmsMessage(fragment);
      messages_[index] = message;
      multiparts_[fragment->part_reference()] = index;
    } else {
      message = messages_[multi_it->second];
      message->add(fragment);
    }
  }
  fragments_[index] = message->index();
}

SmsMessage* SmsCache::get(int index)
{
  std::map<int, SmsMessage *>::iterator it;

  it = messages_.find(index);
  if (it != messages_.end())
    return it->second;
  else
    return NULL;
}

int SmsCache::canonical(int index)
{
  std::map<int, int>::iterator it;

  it = fragments_.find(index);
  if (it != fragments_.end())
    return it->second;
  else
    return -1;
}

void SmsCache::remove(int index)
{
  std::map<int, SmsMessage *>::iterator it;

  it = messages_.find(index);
  if (it == messages_.end())
    return;

  SmsMessage *sms = it->second;
  if (sms->part_count() > 1)
    multiparts_.erase(sms->part_reference());

  std::vector<int> parts = sms->index_list();
  for (std::vector<int>::iterator it = parts.begin();
       it != parts.end();
       ++it)
    fragments_.erase(*it);

  messages_.erase(index);
  delete sms;
}

void SmsCache::clear()
{
  for (std::map<int, SmsMessage *>::iterator message_it = messages_.begin();
       message_it != messages_.end();
       ++message_it)
    delete message_it->second;
  messages_.clear();
  multiparts_.clear();
  fragments_.clear();
}

SmsCache::~SmsCache()
{
  clear();
}

SmsMessage* SmsCache::SmsReceived(int index,
                                  DBus::Error& error,
                                  SmsModemOperations* impl)
{
  SmsMessageFragment* frag = impl->GetSms(index, error);
  if (error.is_set())
    return NULL;

  put(frag);
  return get(canonical(index));
}

static utilities::DBusPropertyMap SmsToPropertyMap(SmsMessage *sms)
{
  utilities::DBusPropertyMap result;

  result["number"].writer().append_string(sms->sender_address().c_str());
  result["smsc"].writer().append_string(sms->smsc_address().c_str());
  result["text"].writer().append_string(sms->text().c_str());
  result["timestamp"].writer().append_string(sms->timestamp().c_str());
  result["index"].writer().append_uint32(sms->index());

  return result;
}

utilities::DBusPropertyMap SmsCache::Get(int index,
                                         DBus::Error& error,
                                         SmsModemOperations* impl)
{
  utilities::DBusPropertyMap result;
  int cindex = canonical(index);

  if (cindex == -1) {
    SmsMessageFragment *frag = impl->GetSms(index, error);
    if (error.is_set())
      return result;
    put(frag);
    cindex = canonical(index);
  }

  SmsMessage *sms = get(cindex);
  if (!sms->is_complete()) {
    LOG(WARNING) << "Message at index " << index << " was not complete.";
    error.set("Message is incomplete", "GetSMS");
    return result;
  }

  return SmsToPropertyMap(sms);
}

static const char* kErrorInvalidIndex =
  "org.freedesktop.ModemManager.Modem.Gsm."
  MM_ERROR_MODEM_GSM_INVALIDINDEX;

void SmsCache::Delete(int index, DBus::Error& error, SmsModemOperations* impl)
{
  SmsMessage* sms = get(index);
  if (sms == NULL) {
    if (canonical(index) == -1) {
      // We don't know anything about this index number. Pass the
      // delete operation through.
      impl->DeleteSms(index, error);
    } else {
      // We know about this index number but it's not valid to delete
      // the middle of multipart messages.
      error.set(kErrorInvalidIndex, "DeleteSMS");
      return;
    }
  } else {
    std::vector<int> slots = sms->index_list();
    // There's some difficulty in handling errors vs. cache
    // consistency here. If we call .remove() unconditionally, and
    // then have an error deleting a message, then the cache will not
    // know about some fragments. The alternative would be to add some
    // API to delete single elements as we go, which is OK for
    // consistency, but we'd have to figure out what to do with the
    // 'key' index of multipart messages. Ensure we delete it last?
    // Wicked complicated.
    remove(index);
    for (std::vector<int>::iterator it = slots.begin();
         it != slots.end();
         ++it) {
      DBus::Error tmperror;
      impl->DeleteSms(*it, tmperror);
    }
  }
}

std::vector<utilities::DBusPropertyMap> SmsCache::List(DBus::Error& error,
                                                       SmsModemOperations* impl)
{
  std::vector<utilities::DBusPropertyMap> result;

  // Reset the cache
  clear();

  // Refill the cache
  std::vector<int> indexlist = impl->ListSms(error);
  for (std::vector<int>::iterator it = indexlist.begin();
       it != indexlist.end();
       ++it) {
    SmsMessageFragment *frag = impl->GetSms(*it, error);
    if (!error.is_set())
      put(frag);
  }

  // Iterate over the cache and return complete messages
  for (std::map<int, SmsMessage *>::iterator sms_it = messages_.begin();
       sms_it != messages_.end();
       ++sms_it) {
    SmsMessage* sms = sms_it->second;
    if (sms->is_complete())
      result.push_back(SmsToPropertyMap(sms));
  }
  return result;
}
