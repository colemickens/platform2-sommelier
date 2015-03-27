// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BIDL_BIDL_CODE_GENERATOR_H_
#define BIDL_BIDL_CODE_GENERATOR_H_

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/cpp/cpp_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/printer.h>

#include <string>
#include <vector>

using google::protobuf::compiler::GeneratorContext;
using google::protobuf::compiler::cpp::CppGenerator;
using google::protobuf::Descriptor;
using google::protobuf::FieldDescriptor;
using google::protobuf::FileDescriptor;
using google::protobuf::MethodDescriptor;
using google::protobuf::ServiceDescriptor;
using google::protobuf::io::Printer;
using std::string;

class MessageNode {
 public:
  const Descriptor* desc;
  std::string name;
  bool contains_objects;
  bool is_binder;
  bool is_fd;
  bool is_nested;
  bool is_repeated;
  std::vector<MessageNode*> children;
  MessageNode* parent;
};

class BidlCodeGenerator : public CppGenerator {
  virtual bool Generate(const FileDescriptor* file,
                        const string& parameter,
                        GeneratorContext* generator_context,
                        string* error) const;

  bool GenerateHeader(const string& basename,
                      const FileDescriptor* file,
                      GeneratorContext* generator_context) const;
  bool AddServiceToHeader(Printer* printer,
                          const ServiceDescriptor* service) const;
  void PrintStandardHeaders(Printer* printer) const;
  void PrintStandardIncludes(Printer* printer) const;
  bool GenerateSource(const string& basename,
                      const FileDescriptor* file,
                      GeneratorContext* generator_context) const;
  bool AddServiceToSource(Printer* printer,
                          const ServiceDescriptor* service) const;
};

#endif  // BIDL_BIDL_CODE_GENERATOR_H_
