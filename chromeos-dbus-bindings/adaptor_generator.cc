// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/adaptor_generator.h"

#include <string>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>

#include "chromeos-dbus-bindings/dbus_signature.h"
#include "chromeos-dbus-bindings/indented_text.h"
#include "chromeos-dbus-bindings/interface.h"

using base::StringPrintf;
using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

namespace {
const int kScopeOffset = 1;
const int kBlockOffset = 2;
const int kLineContinuationOffset = 4;
}  // namespace

bool AdaptorGenerator::GenerateAdaptor(
    const Interface& interface,
    const base::FilePath& output_file) {
  IndentedText text;
  text.AddLine(StringPrintf("// Automatic generation of interface for %s",
                            interface.name.c_str()));
  string header_guard = GenerateHeaderGuard(output_file.value(),
                                            interface.name);
  text.AddLine(StringPrintf("#ifndef %s", header_guard.c_str()));
  text.AddLine(StringPrintf("#define %s", header_guard.c_str()));
  text.AddLine("#include <string>");
  text.AddLine("#include <vector>");
  text.AddLine("");
  text.AddLine("#include <base/macros.h>");
  text.AddLine("#include <dbus/object_path.h>");
  text.AddLine("#include <chromeos/dbus/dbus_object.h>");
  text.AddLine("#include <chromeos/dbus/exported_object_manager.h>");
  text.AddLine("#include <chromeos/variant_dictionary.h>");
  text.AddLine("");

  vector<string> namespaces;
  string class_name;
  CHECK(GetNamespacesAndClassName(interface.name, &namespaces, &class_name));
  for (const auto& space : namespaces) {
    text.AddLine(StringPrintf("namespace %s {", space.c_str()));
  }
  text.AddLine("");

  string adaptor_name = StringPrintf("%sAdaptor", class_name.c_str());
  text.AddLine(StringPrintf("class %s {", adaptor_name.c_str()));
  text.AddLineWithOffset("public:", kScopeOffset);
  string method_interface = StringPrintf("%sAdaptorMethodInterface",
                                         class_name.c_str());
  text.PushOffset(kBlockOffset);
  AddMethodInterface(interface, method_interface, &text);
  AddConstructor(interface, adaptor_name, method_interface, &text);
  text.AddLine(StringPrintf("virtual ~%s() = default;", adaptor_name.c_str()));
  text.AddLine("virtual void OnRegisterComplete(bool success) {}");
  text.PopOffset();

  text.AddLineWithOffset("private:", kScopeOffset);

  text.PushOffset(kBlockOffset);
  text.AddLine(StringPrintf("%s* interface_;  // Owned by caller.",
                            method_interface.c_str()));
  text.AddLine("chromeos::dbus_utils::DBusObject dbus_object_;");
  text.AddLine(StringPrintf(
      "DISALLOW_COPY_AND_ASSIGN(%s);", adaptor_name.c_str()));
  text.PopOffset();

  text.AddLine("};");
  text.AddLine("");

  for (auto it = namespaces.rbegin(); it != namespaces.rend(); ++it) {
    text.AddLine(StringPrintf("}  // namespace %s", it->c_str()));
  }
  text.AddLine(StringPrintf("#endif  // %s", header_guard.c_str()));

  string contents = text.GetContents();
  int expected_write_return = contents.size();
  if (base::WriteFile(output_file, contents.c_str(), contents.size()) !=
      expected_write_return) {
    LOG(ERROR) << "Failed to write file " << output_file.value();
    return false;
  }
  return true;
}

// static
string AdaptorGenerator::GenerateHeaderGuard(
    const string& filename, const string& interface_name) {
  string guard = StringPrintf("____chrome_dbus_binding___%s__%s",
                              interface_name.c_str(), filename.c_str());
  for (auto& c : guard) {
    if (IsAsciiAlpha(c)) {
      c = base::ToUpperASCII(c);
    } else if (!IsAsciiDigit(c)) {
      c = '_';
    }
  }
  return guard;
}

// static
void AdaptorGenerator::AddConstructor(const Interface& interface,
                                      const string& class_name,
                                      const string& method_interface,
                                      IndentedText *text) {
  IndentedText block;
  block.AddLine(StringPrintf("%s(", class_name.c_str()));
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("chromeos::dbus_utils::ExportedObjectManager* object_manager,");
  block.AddLine("const std::string& object_path,");
  block.AddLine(StringPrintf("%s* interface)  // Owned by caller.",
                             method_interface.c_str()));
  block.AddLine(": interface_(interface),");
  block.PushOffset(kBlockOffset);
  block.AddLine("dbus_object_(");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("object_manager,");
  block.AddLine("object_manager->GetBus(),");
  block.AddLine("dbus::ObjectPath(object_path)) {");
  block.PopOffset();
  block.PopOffset();
  block.PopOffset();
  block.PushOffset(kBlockOffset);
  block.AddLine("auto* itf =");
  block.AddLineWithOffset(StringPrintf(
      "dbus_object_.AddOrGetInterface(\"%s\");", interface.name.c_str()),
      kLineContinuationOffset);
  for (const auto& method : interface.methods) {
    block.AddLine("itf->AddMethodHandler(");
    block.PushOffset(kLineContinuationOffset);
    block.AddLine(StringPrintf("\"%s\",", method.name.c_str()));
    block.AddLine("base::Unretained(interface_),");
    block.AddLine(StringPrintf("&%s::%s);",
                              method_interface.c_str(),
                              method.name.c_str()));
    block.PopOffset();
  }
  block.AddLine("dbus_object_.RegisterAsync(base::Bind(");
  block.AddLineWithOffset(
      StringPrintf("&%s::OnRegisterComplete, base::Unretained(this)));",
                   class_name.c_str()),
      kLineContinuationOffset);
  block.PopOffset();
  block.AddLine("}");
  text->AddBlock(block);
}

// static
void AdaptorGenerator::AddMethodInterface(const Interface& interface,
                                          const string& class_name,
                                          IndentedText *text) {
  IndentedText block;
  block.AddLine(StringPrintf("class %s {", class_name.c_str()));
  block.AddLineWithOffset("public:", kScopeOffset);
  DbusSignature signature;
  block.PushOffset(kBlockOffset);
  for (const auto& method : interface.methods) {
    string return_type("void");
    if (!method.output_arguments.empty()) {
      CHECK_EQ(1UL, method.output_arguments.size())
          << "Method " << method.name << " has "
          << method.output_arguments.size()
          << " output arguments which is invalid.";
      CHECK(signature.Parse(method.output_arguments[0].type, &return_type));
    }
    block.AddLine(StringPrintf("virtual %s %s(",
                               return_type.c_str(), method.name.c_str()));
    block.PushOffset(kLineContinuationOffset);
    string last_argument = "chromeos::ErrorPtr* /* error */";
    for (const auto& argument : method.input_arguments) {
      block.AddLine(last_argument + ",");
      CHECK(signature.Parse(argument.type, &last_argument));
      if (!IsIntegralType(last_argument)) {
        last_argument = StringPrintf("const %s&", last_argument.c_str());
      }
      if (!argument.name.empty()) {
        last_argument.append(StringPrintf(" /* %s */", argument.name.c_str()));
      }
    }
    block.AddLine(last_argument + ") = 0;");
    block.PopOffset();
  }
  block.PopOffset();
  block.AddLine("};");

  text->AddBlock(block);
}

// static
bool AdaptorGenerator::GetNamespacesAndClassName(
    const string& interface_name,
    vector<string>* namespaces,
    string* class_name) {
  vector<string> split_namespaces;
  base::SplitString(interface_name, '.', &split_namespaces);
  if (split_namespaces.size() < 2) {
    LOG(ERROR) << "Interface name must have both a domain and object part "
               << "separated by '.'.  Got " << interface_name << " instead.";
    return false;
  }
  *class_name = split_namespaces.back();
  split_namespaces.pop_back();
  namespaces->swap(split_namespaces);
  return true;
}

// static
bool AdaptorGenerator::IsIntegralType(const string& type) {
  return type.find("::") == std::string::npos;
}

}  // namespace chromeos_dbus_bindings
