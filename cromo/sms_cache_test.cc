// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Unit tests for SMS message caching

#include "cromo/sms_cache.h"

#include <memory>
#include <utility>

#include <base/logging.h>
#include <gtest/gtest.h>

namespace cromo {
namespace {

class FakeModem : public SmsModemOperations {
 public:
  virtual SmsMessageFragment* GetSms(int index,
                                     DBus::Error& error) {  // NOLINT - refs.
    std::map<int, PduData>::const_iterator it;
    it = pdus_.find(index);
    if (it == pdus_.end()) {
      error.set("org.freedesktop.ModemManager.Modem.GSM.InvalidIndex",
                "GetSms");
      return nullptr;
    }
    return SmsMessageFragment::CreateFragment(it->second.pdu, it->second.size,
                                              it->first);
  }

  virtual void DeleteSms(int index, DBus::Error& error) {  // NOLINT - refs.
    std::map<int, PduData>::const_iterator it;
    it = pdus_.find(index);
    if (it == pdus_.end()) {
      error.set("org.freedesktop.ModemManager.Modem.GSM.InvalidIndex",
                "DeleteSms");
      return;
    }
    pdus_.erase(index);
  }

  virtual std::vector<int>* ListSms(DBus::Error& error) {  // NOLINT - refs.
    std::vector<int>* ret = new std::vector<int>();
    std::map<int, PduData>::const_iterator it;
    for (it = pdus_.begin(); it != pdus_.end(); ++it)
      ret->push_back(it->first);
    return ret;
  }

  void Add(int index, const uint8_t* pdu, size_t size) {
    PduData element(pdu, size);
    pdus_.insert(std::pair<int, PduData>(index, element));
  }

  bool Contains(int index) {
    std::map<int, PduData>::const_iterator it;
    it = pdus_.find(index);
    return (it != pdus_.end());
  }

 private:
  struct PduData {
    const uint8_t* pdu;
    size_t size;
    PduData(const uint8_t* init_pdu, size_t init_size)
        : pdu(init_pdu),
          size(init_size) {
    }
  };

  std::map<int, PduData> pdus_;
};

static const uint8_t kPduHello[] = {
  0x07, 0x91, 0x21, 0x43, 0x65, 0x87, 0x09, 0xf1,
  0x04, 0x0b, 0x91, 0x81, 0x00, 0x55, 0x15, 0x12,
  0xf2, 0x00, 0x00, 0x11, 0x10, 0x10, 0x21, 0x43,
  0x65, 0x00, 0x0a, 0xe8, 0x32, 0x9b, 0xfd, 0x46,
  0x97, 0xd9, 0xec, 0x37
};

TEST(SmsCache, empty) {
  SmsCache cache;
  FakeModem fake;
  DBus::Error noerror;

  std::unique_ptr<std::vector<utilities::DBusPropertyMap>> messages(
      cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(0, messages->size());

  DBus::Error error;
  std::unique_ptr<utilities::DBusPropertyMap> message(
      cache.Get(1, error, &fake));
  ASSERT_FALSE(message.get());
  ASSERT_TRUE(error.is_set());
}

TEST(SmsCache, hello_get_list) {
  SmsCache cache;
  FakeModem fake;

  const int index = 1;
  fake.Add(index, kPduHello, sizeof(kPduHello));

  DBus::Error noerror;
  std::unique_ptr<utilities::DBusPropertyMap> message(
      cache.Get(index, noerror, &fake));
  ASSERT_TRUE(message.get());
  ASSERT_FALSE(noerror.is_set());

  EXPECT_EQ(index, (*message)["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", (*message)["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", (*message)["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00",
               (*message)["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", (*message)["text"].reader().get_string());

  std::unique_ptr<std::vector<utilities::DBusPropertyMap>> messages(
      cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(1, messages->size());

  EXPECT_EQ(index, (*messages)[0]["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", (*messages)[0]["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", (*messages)[0]["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00",
               (*messages)[0]["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", (*messages)[0]["text"].reader().get_string());

  cache.Delete(index, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.Contains(index));

  DBus::Error error;
  message.reset(cache.Get(index, error, &fake));
  ASSERT_FALSE(message.get());
  ASSERT_TRUE(error.is_set());

  messages.reset(cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(0, messages->size());
}

// Test calling List before Get, since List should cache everything
// and thus cause Get to use a different code path.
TEST(SmsCache, hello_list_get) {
  SmsCache cache;
  FakeModem fake;

  const int index = 1;
  fake.Add(index, kPduHello, sizeof(kPduHello));

  DBus::Error noerror;
  std::unique_ptr<std::vector<utilities::DBusPropertyMap>> messages(
      cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(1, messages->size());

  EXPECT_EQ(index, (*messages)[0]["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", (*messages)[0]["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", (*messages)[0]["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00",
               (*messages)[0]["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", (*messages)[0]["text"].reader().get_string());

  std::unique_ptr<utilities::DBusPropertyMap> message(
      cache.Get(index, noerror, &fake));
  ASSERT_TRUE(message.get());
  ASSERT_FALSE(noerror.is_set());

  EXPECT_EQ(index, (*message)["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", (*message)["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", (*message)["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00",
               (*message)["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", (*message)["text"].reader().get_string());

  cache.Delete(index, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.Contains(1));

  messages.reset(cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(0, messages->size());

  DBus::Error error;
  message.reset(cache.Get(index, error, &fake));
  ASSERT_FALSE(message.get());
  ASSERT_TRUE(error.is_set());
}

static const uint8_t kPduPart1of2[] = {
  0x07, 0x91, 0x41, 0x40, 0x54, 0x05, 0x10, 0xf0,
  0x44, 0x0b, 0x91, 0x61, 0x71, 0x05, 0x64, 0x29,
  0xf5, 0x00, 0x00, 0x11, 0x01, 0x52, 0x41, 0x04,
  0x41, 0x8a, 0xa0, 0x05, 0x00, 0x03, 0x9c, 0x02,
  0x01, 0xa8, 0xe8, 0xf4, 0x1c, 0x94, 0x9e, 0x83,
  0xc2, 0x20, 0x7a, 0x79, 0x4e, 0x77, 0x29, 0x82,
  0xa0, 0x3b, 0x3a, 0x4c, 0xff, 0x81, 0x82, 0x20,
  0x7a, 0x79, 0x4e, 0x77, 0x81, 0xa8, 0xe8, 0xf4,
  0x1c, 0x94, 0x9e, 0x83, 0xde, 0x6e, 0x76, 0x1e,
  0x14, 0x06, 0xd1, 0xcb, 0x73, 0xba, 0x4b, 0x01,
  0xa2, 0xa2, 0xd3, 0x73, 0x50, 0x7a, 0x0e, 0x0a,
  0x83, 0xe8, 0xe5, 0x39, 0xdd, 0xa5, 0x08, 0x82,
  0xee, 0xe8, 0x30, 0xfd, 0x07, 0x0a, 0x82, 0xe8,
  0xe5, 0x39, 0xdd, 0x05, 0xa2, 0xa2, 0xd3, 0x73,
  0x50, 0x7a, 0x0e, 0x7a, 0xbb, 0xd9, 0x79, 0x50,
  0x18, 0x44, 0x2f, 0xcf, 0xe9, 0x2e, 0x05, 0x88,
  0x8a, 0x4e, 0xcf, 0x41, 0xe9, 0x39, 0x28, 0x0c,
  0xa2, 0x97, 0xe7, 0x74, 0x97, 0x22, 0x08, 0xba,
  0xa3, 0xc3, 0xf4, 0x1f, 0x28, 0x08, 0xa2, 0x97,
  0xe7, 0x74, 0x17, 0x88, 0x8a, 0x4e, 0xcf, 0x41,
  0xe9, 0x39, 0xe8, 0xed, 0x66, 0xe7, 0x41
};

static const uint8_t kPduPart2of2[] = {
  0x07, 0x91, 0x41, 0x40, 0x54, 0x05, 0x10, 0xf1,
  0x44, 0x0b, 0x91, 0x61, 0x71, 0x05, 0x64, 0x29,
  0xf5, 0x00, 0x00, 0x11, 0x01, 0x52, 0x41, 0x04,
  0x51, 0x8a, 0x1d, 0x05, 0x00, 0x03, 0x9c, 0x02,
  0x02, 0xc2, 0x20, 0x7a, 0x79, 0x4e, 0x77, 0x81,
  0xa6, 0xe5, 0xf1, 0xdb, 0x4d, 0x06, 0xb5, 0xcb,
  0xf3, 0x79, 0xf8, 0x5c, 0x06,
};

TEST(SmsCache, twopart) {
  SmsCache cache;
  FakeModem fake;

  fake.Add(1, kPduPart1of2, sizeof(kPduPart1of2));
  fake.Add(2, kPduPart2of2, sizeof(kPduPart2of2));

  DBus::Error noerror;
  std::unique_ptr<std::vector<utilities::DBusPropertyMap>> messages(
      cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(1, messages->size());

  EXPECT_EQ(1, (*messages)[0]["index"].reader().get_uint32());
  EXPECT_STREQ("+14044550010", (*messages)[0]["smsc"].reader().get_string());
  EXPECT_STREQ("+16175046925", (*messages)[0]["number"].reader().get_string());
  EXPECT_STREQ("111025144014-07",
               (*messages)[0]["timestamp"].reader().get_string());
  static const char kSmsText[] =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test. Second message";
  EXPECT_STREQ(kSmsText, (*messages)[0]["text"].reader().get_string());

  cache.Delete(1, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.Contains(1));
  EXPECT_FALSE(fake.Contains(2));

  DBus::Error error;
  std::unique_ptr<utilities::DBusPropertyMap> message(
      cache.Get(1, error, &fake));
  ASSERT_FALSE(message.get());
  ASSERT_TRUE(error.is_set());

  messages.reset(cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(0, messages->size());
}

// Test that the cache's assembly order doesn't depend on the order of
// the messages.
TEST(SmsCache, twopart_reverse) {
  SmsCache cache;
  FakeModem fake;

  fake.Add(1, kPduPart2of2, sizeof(kPduPart2of2));
  fake.Add(2, kPduPart1of2, sizeof(kPduPart1of2));

  DBus::Error noerror;
  std::unique_ptr<std::vector<utilities::DBusPropertyMap>> messages(
      cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(1, messages->size());

  EXPECT_EQ(1, (*messages)[0]["index"].reader().get_uint32());
  EXPECT_STREQ("+14044550011", (*messages)[0]["smsc"].reader().get_string());
  EXPECT_STREQ("+16175046925", (*messages)[0]["number"].reader().get_string());
  EXPECT_STREQ("111025144015-07",
               (*messages)[0]["timestamp"].reader().get_string());
  static const char kSmsText[] =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test. Second message";
  EXPECT_STREQ(kSmsText, (*messages)[0]["text"].reader().get_string());

  cache.Delete(1, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.Contains(1));
  EXPECT_FALSE(fake.Contains(2));

  DBus::Error error;
  std::unique_ptr<utilities::DBusPropertyMap> message(
      cache.Get(1, error, &fake));
  ASSERT_FALSE(message.get());
  ASSERT_TRUE(error.is_set());

  messages.reset(cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(0, messages->size());
}

// Test that the cache doesn't get confused when fragments are duplicated
TEST(SmsCache, twopart_duplicate) {
  SmsCache cache;
  FakeModem fake;

  fake.Add(1, kPduPart1of2, sizeof(kPduPart1of2));
  fake.Add(2, kPduPart2of2, sizeof(kPduPart2of2));
  fake.Add(3, kPduPart2of2, sizeof(kPduPart2of2));

  DBus::Error noerror;
  std::unique_ptr<std::vector<utilities::DBusPropertyMap>> messages(
      cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(1, messages->size());

  EXPECT_EQ(1, (*messages)[0]["index"].reader().get_uint32());
  EXPECT_STREQ("+14044550010", (*messages)[0]["smsc"].reader().get_string());
  EXPECT_STREQ("+16175046925", (*messages)[0]["number"].reader().get_string());
  EXPECT_STREQ("111025144014-07",
               (*messages)[0]["timestamp"].reader().get_string());
  static const char kSmsText[] =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test. Second message";
  EXPECT_STREQ(kSmsText, (*messages)[0]["text"].reader().get_string());

  cache.Delete(1, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.Contains(1));
  EXPECT_FALSE(fake.Contains(2));

  DBus::Error error;
  std::unique_ptr<utilities::DBusPropertyMap> message(
      cache.Get(1, error, &fake));
  ASSERT_FALSE(message.get());
  ASSERT_TRUE(error.is_set());

  messages.reset(cache.List(noerror, &fake));
  ASSERT_FALSE(noerror.is_set());
  ASSERT_TRUE(messages.get());
  ASSERT_EQ(0, messages->size());
}

}  // namespace
}  // namespace cromo
