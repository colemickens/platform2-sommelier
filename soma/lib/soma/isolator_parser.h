// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SOMA_LIB_SOMA_ISOLATOR_PARSER_H_
#define SOMA_LIB_SOMA_ISOLATOR_PARSER_H_

#include <map>
#include <memory>
#include <string>

namespace base {
class DictionaryValue;
class ListValue;
}

namespace soma {

class ContainerSpec;

namespace parser {

class UserdbInterface;

// ContainerSpecReader will need to parse many different kinds of "isolators",
// each of which is a dictionary that has a 'name' field and a custom 'value'
// field. I need custom logic for a lot of these to parse the 'value' field,
// but I always pretty much just want to look at the name, invoke the right
// parsing code for the value, and then stuff it into a ContainerSpec.
//
// Creating this interface allows ContainerSpecReader to just have a map of
// isolator name -> IsolatorParserInterfeace implementation that it can
// use to run the right code as it iterates through the list of isolators.
class IsolatorParserInterface {
 public:
  static const char kNameKey[];
  static const char kValueKey[];

  virtual ~IsolatorParserInterface() = default;

  virtual bool Parse(const base::DictionaryValue& value,
                     ContainerSpec* spec) = 0;
};

class IsolatorSetParser : public IsolatorParserInterface {
 public:
  static const char kKey[];

  IsolatorSetParser() = default;
  ~IsolatorSetParser() override = default;

  bool Parse(const base::DictionaryValue& value, ContainerSpec* spec) override;

 protected:
  virtual bool ParseInternal(const base::ListValue& value,
                             ContainerSpec* spec) = 0;
};

class IsolatorObjectParser : public IsolatorParserInterface {
 public:
  IsolatorObjectParser() = default;
  ~IsolatorObjectParser() override = default;

  bool Parse(const base::DictionaryValue& value, ContainerSpec* spec) override;

 protected:
  virtual bool ParseInternal(const base::DictionaryValue& value,
                             ContainerSpec* spec) = 0;
};

class DevicePathFilterParser : public IsolatorSetParser {
 public:
  static const char kName[];

  DevicePathFilterParser() = default;
  ~DevicePathFilterParser() override = default;

 protected:
  bool ParseInternal(const base::ListValue& value,
                     ContainerSpec* spec) override;
};

class DeviceNodeFilterParser : public IsolatorSetParser {
 public:
  static const char kName[];

  DeviceNodeFilterParser() = default;
  ~DeviceNodeFilterParser() override = default;

 protected:
  bool ParseInternal(const base::ListValue& value,
                     ContainerSpec* spec) override;
};

class NamespacesParser : public IsolatorSetParser {
 public:
  static const char kName[];

  NamespacesParser() = default;
  ~NamespacesParser() override = default;

 protected:
  bool ParseInternal(const base::ListValue& value,
                     ContainerSpec* spec) override;
};

class ACLParser : public IsolatorObjectParser {
 public:
  static const char kServiceKey[];
  static const char kWhitelistKey[];

  explicit ACLParser(UserdbInterface* userdb);
  ~ACLParser() override = default;

 protected:
  bool ParseInternal(const base::DictionaryValue& value,
                     ContainerSpec* spec) override = 0;

  UserdbInterface* const userdb_;
};

class UserACLParser : public ACLParser {
 public:
  static const char kName[];

  explicit UserACLParser(UserdbInterface* userdb);
  ~UserACLParser() override = default;

 protected:
  bool ParseInternal(const base::DictionaryValue& value,
                     ContainerSpec* spec) override;
};

class GroupACLParser : public ACLParser {
 public:
  static const char kName[];

  explicit GroupACLParser(UserdbInterface* userdb);
  ~GroupACLParser() override = default;

 protected:
  bool ParseInternal(const base::DictionaryValue& value,
                     ContainerSpec* spec) override;
};

using IsolatorParserMap =
    std::map<std::string, std::unique_ptr<IsolatorParserInterface>>;

}  // namespace parser
}  // namespace soma
#endif  // SOMA_LIB_SOMA_ISOLATOR_PARSER_H_
