// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos-dbus-bindings/adaptor_generator.h"

#include <string>

#include <base/logging.h>
#include <base/strings/stringprintf.h>

#include "chromeos-dbus-bindings/dbus_signature.h"
#include "chromeos-dbus-bindings/indented_text.h"
#include "chromeos-dbus-bindings/interface.h"

using base::StringPrintf;
using std::string;
using std::vector;

namespace chromeos_dbus_bindings {

// static
bool AdaptorGenerator::GenerateAdaptor(
    const Interface& interface,
    const base::FilePath& output_file) {
  IndentedText text;
  text.AddLine(StringPrintf("// Automatic generation of interface for %s",
                            interface.name.c_str()));
  string header_guard = GenerateHeaderGuard(output_file, interface.name);
  text.AddLine(StringPrintf("#ifndef %s", header_guard.c_str()));
  text.AddLine(StringPrintf("#define %s", header_guard.c_str()));
  text.AddLine("#include <string>");
  text.AddLine("#include <vector>");
  text.AddBlankLine();
  text.AddLine("#include <base/macros.h>");
  text.AddLine("#include <dbus/object_path.h>");
  text.AddLine("#include <chromeos/any.h>");
  text.AddLine("#include <chromeos/dbus/dbus_object.h>");
  text.AddLine("#include <chromeos/dbus/exported_object_manager.h>");
  text.AddLine("#include <chromeos/variant_dictionary.h>");
  text.AddBlankLine();

  vector<string> namespaces;
  string class_name;
  CHECK(GetNamespacesAndClassName(interface.name, &namespaces, &class_name));
  for (const auto& space : namespaces) {
    text.AddLine(StringPrintf("namespace %s {", space.c_str()));
  }
  text.AddBlankLine();

  string adaptor_name = StringPrintf("%sAdaptor", class_name.c_str());
  text.AddLine(StringPrintf("class %s {", adaptor_name.c_str()));
  text.AddLineWithOffset("public:", kScopeOffset);

  text.PushOffset(kBlockOffset);
  AddMethodInterface(interface, &text);
  AddConstructor(interface, adaptor_name, &text);
  AddSendSignalMethods(interface, &text);
  text.AddLine(StringPrintf("virtual ~%s() = default;", adaptor_name.c_str()));
  text.AddLine("virtual void OnRegisterComplete(bool success) {}");
  text.PopOffset();

  text.AddBlankLine();
  text.AddLineWithOffset("protected:", kScopeOffset);
  text.PushOffset(kBlockOffset);
  text.AddLine("chromeos::dbus_utils::DBusInterface* dbus_interface() {");
  text.PushOffset(kBlockOffset);
  text.AddLine("return dbus_interface_;");
  text.PopOffset();
  text.AddLine("}");
  text.PopOffset();

  text.AddBlankLine();
  text.AddLineWithOffset("private:", kScopeOffset);

  text.PushOffset(kBlockOffset);
  text.AddLine("MethodInterface* interface_;  // Owned by caller.");
  text.AddLine("chromeos::dbus_utils::DBusObject dbus_object_;");
  AddSignalDataMembers(interface, &text);
  text.AddLine("// Owned by |dbus_object_|.");
  text.AddLine("chromeos::dbus_utils::DBusInterface* dbus_interface_;");
  text.AddLine(StringPrintf(
      "DISALLOW_COPY_AND_ASSIGN(%s);", adaptor_name.c_str()));
  text.PopOffset();

  text.AddLine("};");
  text.AddBlankLine();

  for (auto it = namespaces.rbegin(); it != namespaces.rend(); ++it) {
    text.AddLine(StringPrintf("}  // namespace %s", it->c_str()));
  }
  text.AddLine(StringPrintf("#endif  // %s", header_guard.c_str()));

  return WriteTextToFile(output_file, text);
}

// static
void AdaptorGenerator::AddConstructor(const Interface& interface,
                                      const string& class_name,
                                      IndentedText *text) {
  IndentedText block;
  block.AddLine(StringPrintf("%s(", class_name.c_str()));
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("chromeos::dbus_utils::ExportedObjectManager* object_manager,");
  block.AddLine("const scoped_refptr<dbus::Bus>& bus,");
  block.AddLine("const std::string& object_path,");
  block.AddLine("MethodInterface* interface)  // Owned by caller.");
  block.AddLine(": interface_(interface),");
  block.PushOffset(kBlockOffset);
  block.AddLine("dbus_object_(");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine("object_manager,");
  block.AddLine("bus,");
  block.AddLine("dbus::ObjectPath(object_path)),");
  block.PopOffset();

  // Signal constructors.
  for (const auto& signal : interface.signals) {
    block.AddLine(StringPrintf("signal_%s_(", signal.name.c_str()));
    block.PushOffset(kLineContinuationOffset);
    block.AddLine("&dbus_object_,");
    block.AddLine(StringPrintf("\"%s\",", interface.name.c_str()));
    block.AddLine(StringPrintf("\"%s\"),", signal.name.c_str()));
    block.PopOffset();
  }

  block.AddLine("dbus_interface_(");
  block.PushOffset(kLineContinuationOffset);
  block.AddLine(StringPrintf(
      "dbus_object_.AddOrGetInterface(\"%s\")) {", interface.name.c_str()));
  block.PopOffset();
  block.PopOffset();
  block.PopOffset();
  block.PushOffset(kBlockOffset);
  for (const auto& method : interface.methods) {
    if (method.output_arguments.size() > 1) {
      // TODO(pstew): Accept multiple output arguments.  crbug.com/419271
      continue;
    }
    block.AddLine("dbus_interface_->AddMethodHandler(");
    block.PushOffset(kLineContinuationOffset);
    block.AddLine(StringPrintf("\"%s\",", method.name.c_str()));
    block.AddLine("base::Unretained(interface_),");
    block.AddLine(StringPrintf("&MethodInterface::%s);", method.name.c_str()));
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
                                          IndentedText *text) {
  IndentedText block;
  block.AddLine("class MethodInterface {");
  block.AddLineWithOffset("public:", kScopeOffset);
  DbusSignature signature;
  block.PushOffset(kBlockOffset);
  for (const auto& method : interface.methods) {
    string return_type("void");
    if (!method.output_arguments.empty()) {
      if (method.output_arguments.size() > 1) {
        // TODO(pstew): Accept multiple output arguments.  crbug.com://419271
        LOG(WARNING) << "Method " << method.name << " has "
                     << method.output_arguments.size()
                     << " output arguments which is unsupported.";
        continue;
      }
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
void AdaptorGenerator::AddSendSignalMethods(const Interface& interface,
                                            IndentedText *text) {
  IndentedText block;
  DbusSignature signature;

  for (const auto& signal : interface.signals) {
    block.AddLine(StringPrintf("void Send%sSignal(", signal.name.c_str()));
    block.PushOffset(kLineContinuationOffset);
    string last_argument;
    int unnamed_args = 0;
    string call_arguments;  // The arguments to pass to the Send() call.
    for (const auto& argument : signal.arguments) {
      if (!last_argument.empty())
        block.AddLine(last_argument + ",");
      CHECK(signature.Parse(argument.type, &last_argument));
      if (!IsIntegralType(last_argument))
        last_argument = StringPrintf("const %s&", last_argument.c_str());
      string argument_name = argument.name;
      if (argument.name.empty())
        argument_name = StringPrintf("_arg_%d", ++unnamed_args);
      last_argument.append(" " + argument_name);
      if (!call_arguments.empty())
        call_arguments.append(", ");
      call_arguments.append(argument_name);
    }
    block.AddLine(last_argument + ") {");
    block.PopOffset();
    block.PushOffset(kBlockOffset);
    block.AddLine(StringPrintf(
        "signal_%s_.Send(%s);", signal.name.c_str(), call_arguments.c_str()));
    block.PopOffset();
    block.AddLine("}");
  }
  text->AddBlock(block);
}

// static
void AdaptorGenerator::AddSignalDataMembers(const Interface& interface,
                                            IndentedText *text) {
  IndentedText block;
  DbusSignature signature;

  for (const auto& signal : interface.signals) {
    block.AddLine("chromeos::dbus_utils::DBusSignal<");
    block.PushOffset(kLineContinuationOffset);
    string last_argument;
    for (const auto& argument : signal.arguments) {
      if (!last_argument.empty())
        block.AddLine(last_argument + ",");
      CHECK(signature.Parse(argument.type, &last_argument));
      if (!argument.name.empty())
        last_argument.append(StringPrintf(" /* %s */", argument.name.c_str()));
    }
    block.AddLine(StringPrintf(
        "%s> signal_%s_;", last_argument.c_str(), signal.name.c_str()));
    block.PopOffset();
  }
  text->AddBlock(block);
}



}  // namespace chromeos_dbus_bindings
