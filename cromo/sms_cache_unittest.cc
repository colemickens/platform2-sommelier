// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Unit tests for SMS message caching

#include "sms_cache.h"

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <gtest/gtest.h>

class FakeModem : public SmsModemOperations {
 private:
  struct PduData {
    const uint8_t* pdu_;
    size_t size_;
    PduData(const uint8_t *pdu, size_t size) :
        pdu_(pdu),
        size_(size) {}
  };

  std::map<int, PduData> pdus_;
 public:
  virtual SmsMessageFragment* GetSms(int index, DBus::Error& error) {
    std::map<int, PduData>::iterator it;
    it = pdus_.find(index);
    if (it == pdus_.end()) {
      error.set("org.freedesktop.ModemManager.Modem.GSM.InvalidIndex",
                "GetSms");
      return NULL;
    }
    return SmsMessageFragment::CreateFragment(it->second.pdu_, it->second.size_,
                                              it->first);
  }

  virtual void DeleteSms(int index, DBus::Error& error) {
    std::map<int, PduData>::iterator it;
    it = pdus_.find(index);
    if (it == pdus_.end()) {
      error.set("org.freedesktop.ModemManager.Modem.GSM.InvalidIndex",
                "DeleteSms");
      return;
    }
    pdus_.erase(index);
  }

  virtual std::vector<int> ListSms(DBus::Error& error) {
    std::vector<int> ret;
    std::map<int, PduData>::iterator it;
    for (it = pdus_.begin(); it != pdus_.end(); ++it)
      ret.push_back(it->first);
    return ret;
  }

  void add(int index, const uint8_t *pdu, size_t size) {
    PduData element(pdu, size);
    pdus_.insert(std::pair<int, PduData>(index, element));
  }

  bool contains(int index) {
    std::map<int, PduData>::iterator it;
    it = pdus_.find(index);
    return (it != pdus_.end());
  }
};

static const uint8_t pdu_hello[] = {
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

  std::vector<utilities::DBusPropertyMap> messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(0, messages.size());

  DBus::Error error;
  cache.Get(1, error, &fake);
  ASSERT_TRUE(error.is_set());
}

TEST(SmsCache, hello_get_list) {
  SmsCache cache;
  FakeModem fake;
  std::vector<utilities::DBusPropertyMap> messages;
  utilities::DBusPropertyMap message;
  DBus::Error noerror;
  int index;

  index = 1;
  fake.add(index, pdu_hello, sizeof(pdu_hello));

  message = cache.Get(index, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());

  EXPECT_EQ(index, message["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", message["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", message["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00", message["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", message["text"].reader().get_string());

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(1, messages.size());

  message = messages[0];
  EXPECT_EQ(index, message["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", message["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", message["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00", message["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", message["text"].reader().get_string());

  cache.Delete(index, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.contains(index));

  DBus::Error error;
  cache.Get(index, error, &fake);
  ASSERT_TRUE(error.is_set());

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(0, messages.size());
}

// Test calling List before Get, since List should cache everything
// and thus cause Get to use a different code path.
TEST(SmsCache, hello_list_get) {
  SmsCache cache;
  FakeModem fake;
  std::vector<utilities::DBusPropertyMap> messages;
  utilities::DBusPropertyMap message;
  DBus::Error noerror;
  int index;

  index = 1;
  fake.add(index, pdu_hello, sizeof(pdu_hello));

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(1, messages.size());

  message = messages[0];
  EXPECT_EQ(index, message["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", message["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", message["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00", message["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", message["text"].reader().get_string());

  message = cache.Get(index, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());

  EXPECT_EQ(index, message["index"].reader().get_uint32());
  EXPECT_STREQ("+12345678901", message["smsc"].reader().get_string());
  EXPECT_STREQ("+18005551212", message["number"].reader().get_string());
  EXPECT_STREQ("110101123456+00", message["timestamp"].reader().get_string());
  EXPECT_STREQ("hellohello", message["text"].reader().get_string());

  cache.Delete(index, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.contains(1));

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(0, messages.size());

  DBus::Error error;
  cache.Get(index, error, &fake);
  ASSERT_TRUE(error.is_set());
}

static const uint8_t pdu_part1of2[] = {
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

static const uint8_t pdu_part2of2[] = {
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
  std::vector<utilities::DBusPropertyMap> messages;
  utilities::DBusPropertyMap message;
  DBus::Error noerror;

  fake.add(1, pdu_part1of2, sizeof(pdu_part1of2));
  fake.add(2, pdu_part2of2, sizeof(pdu_part2of2));

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(1, messages.size());

  message = messages[0];
  EXPECT_EQ(1, message["index"].reader().get_uint32());
  EXPECT_STREQ("+14044550010", message["smsc"].reader().get_string());
  EXPECT_STREQ("+16175046925", message["number"].reader().get_string());
  EXPECT_STREQ("111025144014-07", message["timestamp"].reader().get_string());
  const char *sms_text =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test. Second message";
  EXPECT_STREQ(sms_text, message["text"].reader().get_string());

  cache.Delete(1, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.contains(1));
  EXPECT_FALSE(fake.contains(2));

  DBus::Error error;
  cache.Get(1, error, &fake);
  ASSERT_TRUE(error.is_set());

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(0, messages.size());
}

// Test that the cache's assembly order doesn't depend on the order of
// the messages.
TEST(SmsCache, twopart_reverse) {
  SmsCache cache;
  FakeModem fake;
  std::vector<utilities::DBusPropertyMap> messages;
  utilities::DBusPropertyMap message;
  DBus::Error noerror;

  fake.add(1, pdu_part2of2, sizeof(pdu_part2of2));
  fake.add(2, pdu_part1of2, sizeof(pdu_part1of2));

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(1, messages.size());

  message = messages[0];
  EXPECT_EQ(1, message["index"].reader().get_uint32());
  EXPECT_STREQ("+14044550011", message["smsc"].reader().get_string());
  EXPECT_STREQ("+16175046925", message["number"].reader().get_string());
  EXPECT_STREQ("111025144015-07", message["timestamp"].reader().get_string());
  const char *sms_text =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test. Second message";
  EXPECT_STREQ(sms_text, message["text"].reader().get_string());

  cache.Delete(1, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.contains(1));
  EXPECT_FALSE(fake.contains(2));

  DBus::Error error;
  cache.Get(1, error, &fake);
  ASSERT_TRUE(error.is_set());

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(0, messages.size());
}

// Test that the cache doesn't get confused when fragments are duplicated
TEST(SmsCache, twopart_duplicate) {
  SmsCache cache;
  FakeModem fake;
  std::vector<utilities::DBusPropertyMap> messages;
  utilities::DBusPropertyMap message;
  DBus::Error noerror;

  fake.add(1, pdu_part1of2, sizeof(pdu_part1of2));
  fake.add(2, pdu_part2of2, sizeof(pdu_part2of2));
  fake.add(3, pdu_part2of2, sizeof(pdu_part2of2));

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(1, messages.size());

  message = messages[0];
  EXPECT_EQ(1, message["index"].reader().get_uint32());
  EXPECT_STREQ("+14044550010", message["smsc"].reader().get_string());
  EXPECT_STREQ("+16175046925", message["number"].reader().get_string());
  EXPECT_STREQ("111025144014-07", message["timestamp"].reader().get_string());
  const char *sms_text =
      "This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test.\n"
      " This is a test.\n"
      "A what? A test. This is only a test. Second message";
  EXPECT_STREQ(sms_text, message["text"].reader().get_string());

  cache.Delete(1, noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  EXPECT_FALSE(fake.contains(1));
  EXPECT_FALSE(fake.contains(2));

  DBus::Error error;
  cache.Get(1, error, &fake);
  ASSERT_TRUE(error.is_set());

  messages = cache.List(noerror, &fake);
  ASSERT_FALSE(noerror.is_set());
  ASSERT_EQ(0, messages.size());
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  google::InitGoogleLogging(argv[0]);
  google::ParseCommandLineFlags(&argc, &argv, true);

  return RUN_ALL_TESTS();
}
