// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if BASE_VER >= 242728
#include <base/strings/string_number_conversions.h>
#else
#include <base/string_number_conversions.h>
#endif
#include <chromeos/utility.h>

#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include <base/logging.h>
#include <dbus/dbus.h>
#include <fcntl.h>

#if BASE_VER >= 242728
using base::DictionaryValue;
using base::ListValue;
#endif

namespace {

bool DBusMessageIterToPrimitiveValue(DBus::MessageIter& iter, Value** result) {
  DBus::MessageIter subiter;
  switch (iter.type()) {
    case DBUS_TYPE_BYTE:
      *result = Value::CreateIntegerValue(iter.get_byte());
      return true;
    case DBUS_TYPE_BOOLEAN:
      *result = Value::CreateBooleanValue(iter.get_bool());
      return true;
    case DBUS_TYPE_INT16:
      *result = Value::CreateIntegerValue(iter.get_int16());
      return true;
    case DBUS_TYPE_UINT16:
      *result = Value::CreateIntegerValue(iter.get_uint16());
      return true;
    case DBUS_TYPE_INT32:
      *result = Value::CreateIntegerValue(iter.get_int32());
      return true;
    case DBUS_TYPE_UINT32:
      *result = Value::CreateStringValue(base::UintToString(iter.get_uint32()));
      return true;
    case DBUS_TYPE_INT64:
      *result = Value::CreateStringValue(
          base::Int64ToString(iter.get_int64()));
      return true;
    case DBUS_TYPE_UINT64:
      *result = Value::CreateStringValue(
          base::Uint64ToString(iter.get_uint64()));
      return true;
    case DBUS_TYPE_DOUBLE:
      *result = Value::CreateDoubleValue(iter.get_double());
      return true;
    case DBUS_TYPE_STRING:
      *result = Value::CreateStringValue(iter.get_string());
      return true;
    case DBUS_TYPE_OBJECT_PATH:
      *result = Value::CreateStringValue(iter.get_path());
      return true;
    case DBUS_TYPE_SIGNATURE:
      *result = Value::CreateStringValue(iter.get_signature());
      return true;
    case DBUS_TYPE_UNIX_FD:
      *result = Value::CreateIntegerValue(iter.get_int32());
      return true;
    case DBUS_TYPE_VARIANT:
      subiter = iter.recurse();
      return chromeos::DBusMessageIterToValue(subiter, result);
    default:
      LOG(ERROR) << "Unhandled primitive type: " << iter.type();
      return false;
  }
}

bool DBusMessageIterToArrayValue(DBus::MessageIter& iter, Value** result) {
  // For an array, create an empty ListValue, then recurse into it.
  ListValue* lv = new ListValue();
  while (!iter.at_end()) {
    Value* subvalue = NULL;
    bool r = chromeos::DBusMessageIterToValue(iter, &subvalue);
    if (!r) {
      delete lv;
      return false;
    }
    lv->Append(subvalue);
    iter++;
  }
  *result = lv;
  return true;
}

bool DBusMessageIterToDictValue(DBus::MessageIter& iter, Value** result) {
  // For a dict, create an empty DictionaryValue, then walk the subcontainers
  // with the key-value pairs, adding them to the dict.
  DictionaryValue* dv = new DictionaryValue();
  while (!iter.at_end()) {
    DBus::MessageIter subiter = iter.recurse();
    Value* key = NULL;
    Value* value = NULL;
    bool r = chromeos::DBusMessageIterToValue(subiter, &key);
    if (!r || key->GetType() != Value::TYPE_STRING) {
      delete dv;
      return false;
    }
    subiter++;
    r = chromeos::DBusMessageIterToValue(subiter, &value);
    if (!r) {
      delete key;
      delete dv;
      return false;
    }
    std::string keystr;
    key->GetAsString(&keystr);
    dv->Set(keystr, value);
    delete key;
    iter++;
  }
  *result = dv;
  return true;
}

};  // namespace

namespace chromeos {

using std::string;

char DecodeChar(char in) {
  in = tolower(in);
  if ((in <= '9') && (in >= '0')) {
    return in - '0';
  } else {
    CHECK_GE(in, 'a');
    CHECK_LE(in, 'f');
    return in - 'a' + 10;
  }
}

string AsciiEncode(const Blob &blob) {
  static const char table[] = "0123456789abcdef";
  string out;
  for (Blob::const_iterator it = blob.begin(); blob.end() != it; ++it) {
    out += table[((*it) >> 4) & 0xf];
    out += table[*it & 0xf];
  }
  CHECK_EQ(blob.size() * 2, out.size());
  return out;
}

Blob AsciiDecode(const string &str) {
  Blob out;
  if (str.size() % 2)
    return out;
  for (string::const_iterator it = str.begin(); it != str.end(); ++it) {
    char append = DecodeChar(*it);
    append <<= 4;

    ++it;

    append |= DecodeChar(*it);
    out.push_back(append);
  }
  CHECK_EQ(out.size() * 2, str.size());
  return out;
}

void* SecureMemset(void *v, int c, size_t n) {
  volatile unsigned char *p = static_cast<unsigned char *>(v);
  while (n--)
    *p++ = c;
  return v;
}

int SafeMemcmp(const void* s1, const void* s2, size_t n) {
  const unsigned char* us1 = static_cast<const unsigned char*>(s1);
  const unsigned char* us2 = static_cast<const unsigned char*>(s2);
  int result = 0;

  if (0 == n)
    return 1;

  /* Code snippet without data-dependent branch due to
   * Nate Lawson (nate@root.org) of Root Labs. */
  while (n--)
    result |= *us1++ ^ *us2++;

  return result != 0;
}

bool DBusMessageToValue(DBus::Message& message, Value** v) {
  DBus::MessageIter r = message.reader();
  return DBusMessageIterToArrayValue(r, v);
}

bool DBusMessageIterToValue(DBus::MessageIter& iter, Value** result) {
  if (iter.at_end()) {
    *result = NULL;
    return true;
  }
  if (!iter.is_array() && !iter.is_dict()) {
    // Primitive type. Stash it in result directly and return.
    return DBusMessageIterToPrimitiveValue(iter, result);
  }
  if (iter.is_dict()) {
    DBus::MessageIter subiter = iter.recurse();
    return DBusMessageIterToDictValue(subiter, result);
  }
  if (iter.is_array()) {
    DBus::MessageIter subiter = iter.recurse();
    return DBusMessageIterToArrayValue(subiter, result);
  }
  return false;
}

bool DBusPropertyMapToValue(std::map<std::string, DBus::Variant>& properties,
                            Value** v) {
  DictionaryValue* dv = new DictionaryValue();
  for (std::map<std::string, DBus::Variant>::iterator it = properties.begin();
       it != properties.end();
       ++it) {
    Value* v = NULL;
    // make a copy so we can take a reference to the MessageIter
    DBus::MessageIter reader = it->second.reader();
    bool r = DBusMessageIterToValue(reader, &v);
    if (!r) {
      delete dv;
      return false;
    }
    dv->Set(it->first, v);
  }
  *v = static_cast<Value*>(dv);
  return true;
}

bool SecureRandom(uint8_t *buf, size_t len) {
  int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0)
    return false;
  if (read(fd, buf, len) != static_cast<ssize_t>(len)) {
    close(fd);
    return false;
  }
  close(fd);
  return true;
}

bool SecureRandomString(size_t len, std::string* result) {
  char rbuf[len];
  if (!SecureRandom(reinterpret_cast<uint8_t*>(rbuf), len))
    return false;
  *result = base::HexEncode(rbuf, len);
  return true;
}

}  // namespace chromeos
