// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <mm/mm-modem.h>  // for MM_ERROR_MODEM_GSM_INVALIDINDEX

#include "sms_cache.h"

namespace cromo {

void SmsCache::AddToCache(SmsMessageFragment* fragment) {
  const int index = fragment->index();

  std::map<int, int>::const_iterator it = fragments_.find(index);
  if (it != fragments_.end()) {
    messages_.erase(it->second);
    fragments_.erase(index);
    // TODO(njw): Need to check for and remove from multipart object
    // as well (though this whole clause is kind of a big
    // shouldn't-happen)
  }

  SmsMessage* message;
  if (fragment->part_count() == 1) {
    message = new SmsMessage(fragment);
    messages_[index] = message;
  } else {
    std::map<uint16_t, int>::const_iterator multi_it;
    multi_it = multiparts_.find(fragment->part_reference());
    if (multi_it == multiparts_.end()) {
      message = new SmsMessage(fragment);
      messages_[index] = message;
      multiparts_[fragment->part_reference()] = index;
    } else {
      message = messages_[multi_it->second];
      message->AddFragment(fragment);
    }
  }
  fragments_[index] = message->index();
}

SmsMessage* SmsCache::GetFromCache(int index) {
  std::map<int, SmsMessage*>::const_iterator it = messages_.find(index);
  if (it != messages_.end())
    return it->second;
  else
    return NULL;
}

int SmsCache::GetCanonicalIndex(int index) {
  std::map<int, int>::const_iterator it = fragments_.find(index);
  if (it != fragments_.end())
    return it->second;
  else
    return -1;
}

void SmsCache::RemoveFromCache(int index) {
  std::map<int, SmsMessage*>::const_iterator it = messages_.find(index);
  if (it == messages_.end())
    return;

  const SmsMessage* sms = it->second;
  if (sms->part_count() > 1)
    multiparts_.erase(sms->part_reference());

  scoped_ptr<std::vector<int> > parts(sms->MessageIndexList());
  for (std::vector<int>::const_iterator it = parts->begin();
       it != parts->end();
       ++it) {
    fragments_.erase(*it);
  }

  messages_.erase(index);
  delete sms;
}

void SmsCache::ClearCache() {
  for (std::map<int, SmsMessage*>::const_iterator message_it =
           messages_.begin();
       message_it != messages_.end();
       ++message_it) {
    delete message_it->second;
  }
  messages_.clear();
  multiparts_.clear();
  fragments_.clear();
}

SmsCache::~SmsCache() {
  ClearCache();
}

SmsMessage* SmsCache::SmsReceived(int index,
                                  DBus::Error& error,
                                  SmsModemOperations* impl) {
  SmsMessageFragment* frag = impl->GetSms(index, error);
  if (error.is_set())
    return NULL;

  AddToCache(frag);
  return GetFromCache(GetCanonicalIndex(index));
}

static void SmsToPropertyMap(SmsMessage* sms,
                             utilities::DBusPropertyMap* result) {
  CHECK(result != NULL);
  (*result)["number"].writer().append_string(sms->sender_address().c_str());
  (*result)["smsc"].writer().append_string(sms->smsc_address().c_str());
  (*result)["text"].writer().append_string(sms->GetMessageText().c_str());
  (*result)["timestamp"].writer().append_string(sms->timestamp().c_str());
  (*result)["index"].writer().append_uint32(sms->index());
}

utilities::DBusPropertyMap* SmsCache::Get(int index,
                                          DBus::Error& error,
                                          SmsModemOperations* impl) {
  int cindex = GetCanonicalIndex(index);

  if (cindex == -1) {
    SmsMessageFragment* frag = impl->GetSms(index, error);
    if (error.is_set())
      return NULL;
    AddToCache(frag);
    cindex = GetCanonicalIndex(index);
  }

  SmsMessage* sms = GetFromCache(cindex);
  if (!sms->IsComplete()) {
    LOG(WARNING) << "Message at index " << index << " was not complete.";
    error.set("Message is incomplete", "GetSMS");
    return NULL;
  }

  utilities::DBusPropertyMap* map = new utilities::DBusPropertyMap();
  SmsToPropertyMap(sms, map);
  return map;
}

static const char kErrorInvalidIndex[] =
    "org.freedesktop.ModemManager.Modem.Gsm."
    MM_ERROR_MODEM_GSM_INVALIDINDEX;

void SmsCache::Delete(int index, DBus::Error& error, SmsModemOperations* impl) {
  const SmsMessage* sms = GetFromCache(index);
  if (sms == NULL) {
    if (GetCanonicalIndex(index) == -1) {
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
    scoped_ptr<std::vector<int> > slots(sms->MessageIndexList());
    // There's some difficulty in handling errors vs. cache
    // consistency here. If we call RemoveFromCache() unconditionally,
    // and then have an error deleting a message, then the cache will
    // not know about some fragments. The alternative would be to add
    // some API to delete single elements as we go, which is OK for
    // consistency, but we'd have to figure out what to do with the
    // 'key' index of multipart messages. Ensure we delete it last?
    // Wicked complicated.
    RemoveFromCache(index);
    for (std::vector<int>::const_iterator it = slots->begin();
         it != slots->end();
         ++it) {
      DBus::Error tmperror;
      impl->DeleteSms(*it, tmperror);
    }
  }
}

std::vector<utilities::DBusPropertyMap>* SmsCache::List(
    DBus::Error& error,
    SmsModemOperations* impl) {
  // Reset the cache
  ClearCache();

  // Refill the cache
  scoped_ptr<std::vector<int> > indexlist(impl->ListSms(error));
  if (!error.is_set()) {
    for (std::vector<int>::const_iterator it = indexlist->begin();
         it != indexlist->end();
         ++it) {
      SmsMessageFragment* frag = impl->GetSms(*it, error);
      if (!error.is_set())
        AddToCache(frag);
    }
  }

  // Iterate over the cache and return complete messages
  std::vector<utilities::DBusPropertyMap>* result =
      new std::vector<utilities::DBusPropertyMap>();
  for (std::map<int, SmsMessage*>::const_iterator sms_it = messages_.begin();
       sms_it != messages_.end();
       ++sms_it) {
    SmsMessage* sms = sms_it->second;
    if (sms->IsComplete()) {
      utilities::DBusPropertyMap map;
      // Copy an empty map into the vector and then operate on it,
      // rather than operating on a map and copying the contents.
      result->push_back(map);
      SmsToPropertyMap(sms, &result->back());
    }
  }
  return result;
}

}  // namespace cromo
