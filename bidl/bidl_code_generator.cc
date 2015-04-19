// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bidl/bidl_code_generator.h"

#include <stdio.h>

#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include <map>
#include <string>
#include <vector>

#include "bidl/utils.h"

using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;
using std::map;

string FullName(const Descriptor* desc) {
  string name;
  vector<string> package_parts;
  SplitStringUsing(desc->file()->package(), ".", &package_parts);

  vector<string> name_parts;
  SplitStringUsing(desc->full_name(), ".", &name_parts);
  size_t i;
  for (i = 0; i < package_parts.size(); i++) {
    name.append("::" + package_parts[i]);
  }
  name.append("::");
  for (; i < name_parts.size() - 1; i++) {
    name.append(name_parts[i] + "_");
  }
  name.append(desc->name());
  return name;
}

string FullNameVaribleName(const Descriptor* desc) {
  string name;
  vector<string> name_parts;
  SplitStringUsing(desc->full_name(), ".", &name_parts);
  size_t i;
  for (i = 0; i < name_parts.size(); i++) {
    name.append("_" + name_parts[i]);
  }
  return name;
}

// Used for debug only
void PrintIndent(int depth) {
  for (int i = 0; i < depth; i++)
    fprintf(stderr, "  ");
}

// Used for debug only
void PrintBinderTree(MessageNode* node, int depth) {
  PrintIndent(depth);
  fprintf(stderr, "Fullname %s: ", FullName(node->desc).c_str());
  if (node->is_nested)
    fprintf(stderr, "N ");
  fprintf(stderr, "%s %d %d %s\n", node->desc->name().c_str(), node->is_fd,
          node->contains_objects, node->name.c_str());
  if (node->is_fd) {
    PrintIndent(depth);
    fprintf(stderr, "DO FD\n");
    return;
  }
  if (node->is_binder) {
    PrintIndent(depth);
    fprintf(stderr, "DO Binder\n");
    return;
  }
  if (node->contains_objects) {
    // Process children
    for (std::vector<MessageNode*>::iterator it = node->children.begin();
         it != node->children.end(); it++) {
      PrintBinderTree(*it, depth + 1);
    }
  }
}

void PrintMarshallCodeForBinderTree(Printer* printer,
                                    MessageNode* node,
                                    int depth) {
  if (node->is_fd || node->is_binder) {
    if (node->parent) {
      if (node->is_repeated) {
        printer->Print(
            "for (size_t i=0; i<message_$message$->$field$_size(); i++) {\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name);
      } else {
        printer->Print("if (message_$message$->has_$field$()) {\n", "message",
                       FullNameVaribleName(node->parent->desc), "field",
                       node->name);
      }
      printer->Indent();
      if (node->is_fd)
        printer->Print(
            "object_parcel.WriteFd(message_$message$->mutable_$field$($index$)-"
            ">fd());"
            "\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name, "index", node->is_repeated ? "i" : "");
      else
        printer->Print(
            "object_parcel.WriteStrongBinder(reinterpret_cast<const "
            "IBinder*>(message_$message$->mutable_$field$($index$)->ibinder()))"
            ";"
            "\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name, "index", node->is_repeated ? "i" : "");
      printer->Print(
          "message_$message$->mutable_$field$($index$)->set_offset(offset);\n",
          "message", FullNameVaribleName(node->parent->desc), "field",
          node->name, "index", node->is_repeated ? "i" : "");
      printer->Print("offset++;\n");
      printer->Outdent();
      printer->Print("}\n");

    } else {
      if (node->is_fd)
        printer->Print("object_parcel.WriteFd(message_$message$->fd());\n",
                       "message", FullNameVaribleName(node->desc));
      else
        printer->Print(
            "object_parcel.WriteStrongBinder(reinterpret_cast<const "
            "IBinder*>(message_$message$->ibinder()));\n",
            "message", FullNameVaribleName(node->desc));
      printer->Print("message_$message$->set_offset(offset);\n", "message",
                     FullNameVaribleName(node->desc));
    }

    return;
  }

  if (node->contains_objects) {
    if (node->parent != NULL) {
      if (node->is_repeated) {
        printer->Print(
            "for (size_t i=0; i<message_$parent$->$field$_size(); i++) {\n",
            "parent", FullNameVaribleName(node->parent->desc), "field",
            node->name);
      } else {
        printer->Print("if (message_$parent$->has_$field$()) {\n", "parent",
                       FullNameVaribleName(node->parent->desc), "field",
                       node->name);
      }
      printer->Indent();

      printer->Print("$name$* message_$varname$ = ", "name",
                     FullName(node->desc), "varname",
                     FullNameVaribleName(node->desc));
      printer->Print("message_$parent$->mutable_$field$($index$);\n", "parent",
                     FullNameVaribleName(node->parent->desc), "field",
                     node->name, "index", node->is_repeated ? "i" : "");
    }
    // Process children
    for (std::vector<MessageNode*>::iterator it = node->children.begin();
         it != node->children.end(); it++) {
      PrintMarshallCodeForBinderTree(printer, *it, depth + 1);
    }
    if (node->parent != NULL) {
      printer->Outdent();
      printer->Print("}\n");
    }
  }

  printer->Print("\n");
}

void PrintUnmarshallCodeForBinderTree(Printer* printer,
                                      MessageNode* node,
                                      int depth,
                                      bool is_reply) {
  if (node->is_fd || node->is_binder) {
    if (node->parent) {
      if (node->is_repeated) {
        printer->Print(
            "for (size_t i=0; i<message_$message$->$field$_size(); i++) {\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name);
      } else {
        printer->Print("if (message_$message$->has_$field$()) {\n", "message",
                       FullNameVaribleName(node->parent->desc), "field",
                       node->name);
      }

      printer->Indent();

      printer->Print("{\n");
      printer->Indent();

      if (node->is_fd) {
        printer->Print("int fd = -1;\n");
        printer->Print("if (!$parcel$GetFdAtOffset(&fd, ", "parcel",
                       is_reply ? "reply." : "data->");
        printer->Print(
            "message_$message$->mutable_$field$($index$)->offset()))\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name, "index", node->is_repeated ? "i" : "");
        printer->Indent();
        printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
        printer->Outdent();

        printer->Print(
            "message_$message$->mutable_$field$($index$)->set_fd(fd);\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name, "index", node->is_repeated ? "i" : "");
      } else {
        printer->Print("IBinder* binder = nullptr;\n");
        printer->Print("if (!$parcel$GetStrongBinderAtOffset(&binder, ",
                       "parcel", is_reply ? "reply." : "data->");
        printer->Print(
            "message_$message$->mutable_$field$($index$)->offset()))\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name, "index", node->is_repeated ? "i" : "");
        printer->Indent();
        printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
        printer->Outdent();

        printer->Print(
            "message_$message$->mutable_$field$($index$)->set_ibinder("
            "reinterpret_cast<uint64_t>(binder));\n",
            "message", FullNameVaribleName(node->parent->desc), "field",
            node->name, "index", node->is_repeated ? "i" : "");
      }
      printer->Outdent();
      printer->Print("}\n");

      printer->Outdent();
      printer->Print("}\n");

    } else {
      if (node->is_fd) {
        printer->Print("int fd = -1;\n");
        printer->Print("if (!$parcel$GetFdAtOffset(&fd, ", "parcel",
                       is_reply ? "reply." : "data->");
        printer->Print("message_$message$->offset()))\n", "message",
                       FullNameVaribleName(node->desc));
        printer->Indent();
        printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
        printer->Outdent();
        printer->Print("message_$message$->set_fd(fd);\n", "message",
                       FullNameVaribleName(node->desc));
      } else {
        printer->Print("IBinder* binder = nullptr;\n");
        printer->Print("if (!$parcel$GetStrongBinderAtOffset(&binder, ",
                       "parcel", is_reply ? "reply." : "data->");
        printer->Print("message_$message$->offset()))\n", "message",
                       FullNameVaribleName(node->desc));
        printer->Indent();
        printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
        printer->Outdent();

        printer->Print(
            "message_$message$->set_ibinder("
            "reinterpret_cast<uint64_t>(binder));\n",
            "message", FullNameVaribleName(node->desc));
      }
    }

    return;
  }

  if (node->contains_objects) {
    if (node->parent != NULL) {
      if (node->is_repeated) {
        printer->Print(
            "for (size_t i=0; i<message_$parent$->$field$_size(); i++) {\n",
            "parent", FullNameVaribleName(node->parent->desc), "field",
            node->name);
      } else {
        printer->Print("if (message_$parent$->has_$field$()) {\n", "parent",
                       FullNameVaribleName(node->parent->desc), "field",
                       node->name);
      }
      printer->Indent();

      printer->Print("$name$* message_$varname$ = ", "name",
                     FullName(node->desc), "varname",
                     FullNameVaribleName(node->desc));
      printer->Print("message_$parent$->mutable_$field$($index$);\n", "parent",
                     FullNameVaribleName(node->parent->desc), "field",
                     node->name, "index", node->is_repeated ? "i" : "");
    }
    // Process children
    for (std::vector<MessageNode*>::iterator it = node->children.begin();
         it != node->children.end(); it++) {
      PrintUnmarshallCodeForBinderTree(printer, *it, depth + 1, is_reply);
    }
    if (node->parent != NULL) {
      printer->Outdent();
      printer->Print("}\n");
    }
  }

  printer->Print("\n");
}

bool FindObjects(MessageNode* node) {
  const Descriptor* message = node->desc;
  node->is_fd = false;
  node->is_binder = false;

  if (message->name() == "BinderFd") {
    node->is_fd = true;
    return true;
  }

  if (message->name() == "StrongBinder") {
    node->is_binder = true;
    return true;
  }

  // Check each Field and look for a message.
  node->contains_objects = false;
  for (int i = 0; i < message->field_count(); i++) {
    const FieldDescriptor* field = message->field(i);

    if (field->type() == FieldDescriptor::TYPE_MESSAGE) {
      MessageNode* new_node = new MessageNode();
      new_node->name = field->name();
      new_node->desc = field->message_type();
      new_node->parent = node;
      new_node->is_nested =
          message->FindNestedTypeByName(new_node->desc->name()) != NULL;
      new_node->is_repeated = field->label() == FieldDescriptor::LABEL_REPEATED;
      node->children.push_back(new_node);
      node->contains_objects |= FindObjects(new_node);
    }
  }
  return node->contains_objects;
}

bool IsOneWay(const Descriptor* desc) {
  return FullName(desc) == "::protobinder::NoResponse";
}

void BidlCodeGenerator::PrintStandardHeaders(Printer* printer) const {
  printer->Print(
      "// Copyright 2015 The Chromium OS Authors. All rights reserved.\n"
      "// Use of this source code is governed by a BSD-style license that can "
      "be\n"
      "// found in the LICENSE file.\n"
      "\n");
}

void BidlCodeGenerator::PrintStandardIncludes(Printer* printer) const {
  printer->Print("#include <protobinder/iinterface.h>\n");
  printer->Print("#include <protobinder/parcel.h>\n");
  printer->Print("#include <protobinder/status.h>\n");
  printer->Print("\n");
  printer->Print("#include <string.h>\n");
  printer->Print("\n");
}

bool BidlCodeGenerator::AddServiceToHeader(
    Printer* printer,
    const ServiceDescriptor* service) const {
  map<string, string> vars;
  vars["classname"] = service->name();
  vars["full_name"] = service->full_name();

  printer->Print(vars,
                 "class I$classname$ : public IInterface {\n"
                 " public:\n");
  printer->Indent();

  int method_count = service->method_count();

  if (method_count > 0) {
    for (int i = 0; i < method_count; i++) {
      const MethodDescriptor* method = service->method(i);
      printer->Print("virtual Status ");
      printer->Print(method->name().c_str());
      printer->Print("(");
      printer->Print(FullName(method->input_type()).c_str());
      if (IsOneWay(method->output_type())) {
        printer->Print("* in) = 0;\n");
      } else {
        printer->Print("* in, ");
        printer->Print(FullName(method->output_type()).c_str());
        printer->Print("* out) = 0;\n");
      }
    }
  }
  printer->Print(vars, "DECLARE_META_INTERFACE($classname$)\n");

  printer->Outdent();
  printer->Print(vars, "};\n\n");

  printer->Print(vars,
                 "class I$classname$HostInterface : public "
                 "BinderHostInterface<I$classname$> {\n"
                 " public:\n");
  printer->Indent();

  printer->Print(vars, "virtual Status OnTransact(uint32_t code,\n");
  printer->Print(vars, "                          Parcel* data,\n");
  printer->Print(vars, "                          Parcel* reply,\n");
  printer->Print(vars, "                          bool one_way) {\n");

  printer->Indent();

  // Extract function name from parcel.
  printer->Print("std::string function_name;\n");
  printer->Print("if (!data->ReadString(&function_name))\n");
  printer->Indent();
  printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
  printer->Outdent();

  // Read the proto data.
  printer->Print("std::string data_string;\n");
  printer->Print("if (!data->ReadString(&data_string))\n");
  printer->Indent();
  printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
  printer->Outdent();

  for (int i = 0; i < method_count; i++) {
    const MethodDescriptor* method = service->method(i);
    printer->Print("if (function_name == \"$name$\") {\n", "name",
                   method->name().c_str());
    printer->Indent();

    printer->Print(FullName(method->input_type()).c_str());
    printer->Print(" in;\n");
    printer->Print("if (!in.ParseFromString(data_string))\n");
    printer->Indent();
    printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PROTO);;\n");
    printer->Outdent();

    MessageNode in_message;
    in_message.name = "root";
    in_message.parent = NULL;
    in_message.is_nested = false;
    in_message.desc = method->input_type();
    FindObjects(&in_message);

    if (in_message.contains_objects) {
      printer->Print("\n");
      printer->Print(FullName(method->input_type()).c_str());
      printer->Print("* message_");
      printer->Print(FullNameVaribleName(method->input_type()).c_str());
      printer->Print(" = &in;\n");
      PrintUnmarshallCodeForBinderTree(printer, &in_message, 0, false);
    }
    if (IsOneWay(method->output_type())) {
      printer->Print("return $name$(&in);\n", "name", method->name());
    } else {
      printer->Print(FullName(method->output_type()).c_str());
      printer->Print(" out;\n");
      printer->Print("Status status = $name$(&in, &out);\n", "name",
                     method->name());
      printer->Print("if (!status.IsOk())\n");
      printer->Indent();
      printer->Print("return status;\n");
      printer->Outdent();

      MessageNode out_message;
      out_message.name = "root";
      out_message.parent = NULL;
      out_message.is_nested = false;
      out_message.desc = method->output_type();
      FindObjects(&out_message);

      // TODO(leecam): Case where root is a binder itself
      if (out_message.contains_objects) {
        printer->Print("\n");
        printer->Print("size_t offset = 0;\n");
        printer->Print("Parcel object_parcel;\n");
        printer->Print(FullName(method->output_type()).c_str());
        printer->Print("* message_");
        printer->Print(FullNameVaribleName(method->output_type()).c_str());
        printer->Print(" = &out;\n");
        PrintMarshallCodeForBinderTree(printer, &out_message, 0);
      }

      printer->Print("std::string reply_string;\n");
      printer->Print("if (!out.SerializeToString(&reply_string))\n");
      printer->Indent();
      printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PROTO);\n");
      printer->Outdent();

      printer->Print("if (!reply->WriteString(reply_string))\n");
      printer->Indent();
      printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
      printer->Outdent();

      if (out_message.contains_objects) {
        printer->Print("if (!reply->WriteParcel(&object_parcel))\n");
        printer->Indent();
        printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
        printer->Outdent();
      }
      printer->Print("return status;\n");
    }

    printer->Outdent();
    printer->Print("}\n");
  }
  printer->Print(
      "return BinderHostInterface::OnTransact(code, data, reply, one_way);\n");

  printer->Outdent();
  printer->Print("}\n");

  printer->Outdent();
  printer->Print("};\n");

  return true;
}

bool BidlCodeGenerator::GenerateHeader(
    const string& basename,
    const FileDescriptor* file,
    GeneratorContext* generator_context) const {
  google::protobuf::scoped_ptr<ZeroCopyOutputStream> output(
      generator_context->Open(basename + ".pb.rpc.h"));
  Printer printer(output.get(), '$');

  vector<string> package_parts;
  SplitStringUsing(file->package(), ".", &package_parts);

  PrintStandardHeaders(&printer);

  printer.Print("#ifndef BIDL_$name$_RPC_H_\n", "name", basename);
  printer.Print("#define BIDL_$name$_RPC_H_\n\n", "name", basename);

  PrintStandardIncludes(&printer);

  printer.Print("#include \"$name$.pb.h\"\n", "name", basename);

  printer.Print("using namespace protobinder;\n\n");

  for (size_t i = 0; i < package_parts.size(); i++) {
    printer.Print("namespace $part$ {\n", "part", package_parts[i]);
  }

  printer.Print("\n");

  // service_count returns int, not size_t :(
  for (int i = 0; i < file->service_count(); i++) {
    // For each RPC service we need to generate IInterfaces
    AddServiceToHeader(&printer, file->service(i));
  }

  for (int i = package_parts.size() - 1; i >= 0; i--) {
    printer.Print("}  // namespace $part$\n", "part", package_parts[i]);
  }

  printer.Print("\n#endif  // BIDL_$name$_RPC_H_\n", "name", basename);

  return true;
}

bool BidlCodeGenerator::AddServiceToSource(
    Printer* printer,
    const ServiceDescriptor* service) const {
  map<string, string> vars;
  vars["classname"] = service->name();
  vars["full_name"] = service->full_name();

  printer->Print(
      vars,
      "class I$classname$Proxy : public BinderProxyInterface<I$classname$> {\n"
      " public:\n");
  printer->Indent();
  printer->Print(vars,
                 "I$classname$Proxy(IBinder* impl) : "
                 "BinderProxyInterface<I$classname$>(impl) {}\n\n");

  int method_count = service->method_count();

  for (int i = 0; i < method_count; i++) {
    const MethodDescriptor* method = service->method(i);
    printer->Print("virtual Status ");
    printer->Print(method->name().c_str());
    printer->Print("(");
    printer->Print(FullName(method->input_type()).c_str());
    if (IsOneWay(method->output_type())) {
      printer->Print("* in) {\n");
    } else {
      printer->Print("* in, ");
      printer->Print(FullName(method->output_type()).c_str());
      printer->Print("* out) {\n");
    }
    printer->Indent();

    MessageNode in_message;
    in_message.name = "root";
    in_message.parent = NULL;
    in_message.is_nested = false;
    in_message.desc = method->input_type();
    FindObjects(&in_message);

    // TODO(leecam): Case where root is a binder itself
    if (in_message.contains_objects) {
      printer->Print("size_t offset = 0;\n");
      printer->Print("Parcel object_parcel;\n");
      printer->Print(FullName(method->input_type()).c_str());
      printer->Print("* message_");
      printer->Print(FullNameVaribleName(method->input_type()).c_str());
      printer->Print(" = in;\n");
      PrintMarshallCodeForBinderTree(printer, &in_message, 0);
    }

    printer->Print("std::string in_string;\n");
    printer->Print("if (!in->SerializeToString(&in_string))\n");
    printer->Indent();
    printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PROTO);\n");
    printer->Outdent();

    printer->Print("Parcel data, reply;\n");

    // Write function name.
    printer->Print("if (!data.WriteString(\"$name$\"))\n", "name",
                   method->name());
    printer->Indent();
    printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
    printer->Outdent();

    // Write proto data.
    printer->Print("if (!data.WriteString(in_string))\n");
    printer->Indent();
    printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
    printer->Outdent();

    if (in_message.contains_objects) {
      printer->Print("if (!data.WriteParcel(&object_parcel))\n");
      printer->Indent();
      printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
      printer->Outdent();
    }

    printer->Print("if (!Remote())\n");
    printer->Indent();
    printer->Print("return STATUS_BINDER_ERROR(Status::ENDPOINT_NOT_SET);\n");
    printer->Outdent();

    if (IsOneWay(method->output_type())) {
      printer->Print("return Remote()->Transact(0, &data, &reply, true);\n");
    } else {
      printer->Print(
          "Status status = Remote()->Transact(0, &data, &reply, false);\n");
      printer->Print("if (!status.IsOk())\n");
      printer->Indent();
      printer->Print("return status;\n");
      printer->Outdent();

      printer->Print("std::string out_string;\n");
      printer->Print("if (!reply.ReadString(&out_string))\n");
      printer->Indent();
      printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PARCEL);\n");
      printer->Outdent();

      printer->Print("if (!out->ParseFromString(out_string))\n");
      printer->Indent();
      printer->Print("return STATUS_BINDER_ERROR(Status::BAD_PROTO);\n");
      printer->Outdent();

      // correct objects
      MessageNode out_message;
      out_message.name = "root";
      out_message.parent = NULL;
      out_message.is_nested = false;
      out_message.desc = method->output_type();
      FindObjects(&out_message);

      if (out_message.contains_objects) {
        printer->Print("\n");
        printer->Print(FullName(method->output_type()).c_str());
        printer->Print("* message_");
        printer->Print(FullNameVaribleName(method->output_type()).c_str());
        printer->Print(" = out;\n");
        PrintUnmarshallCodeForBinderTree(printer, &out_message, 0, true);
      }
      printer->Print("return status;\n");
    }

    printer->Outdent();
    printer->Print("}\n");
  }

  printer->Outdent();
  printer->Print("};\n\n");

  printer->Print(vars,
                 "IMPLEMENT_META_INTERFACE($classname$, \"$classname$\")\n\n");

  return true;
}

bool BidlCodeGenerator::GenerateSource(
    const string& basename,
    const FileDescriptor* file,
    GeneratorContext* generator_context) const {
  google::protobuf::scoped_ptr<ZeroCopyOutputStream> output(
      generator_context->Open(basename + ".pb.rpc.cc"));
  Printer printer(output.get(), '$');

  vector<string> package_parts;
  SplitStringUsing(file->package(), ".", &package_parts);

  PrintStandardHeaders(&printer);

  printer.Print("#include \"$name$.pb.rpc.h\"\n\n", "name", basename);

  for (size_t i = 0; i < package_parts.size(); i++) {
    printer.Print("namespace $part$ {\n", "part", package_parts[i]);
  }

  printer.Print("\n");

  // service_count returns int, not size_t :(
  for (int i = 0; i < file->service_count(); i++) {
    // For each RPC service we need to generate Proxies
    AddServiceToSource(&printer, file->service(i));
  }

  printer.Print("\n");

  for (int i = package_parts.size() - 1; i >= 0; i--) {
    printer.Print("}  // namespace $part$\n", "part", package_parts[i]);
  }

  return true;
}

bool BidlCodeGenerator::Generate(const FileDescriptor* file,
                                 const string& parameter,
                                 GeneratorContext* generator_context,
                                 string* error) const {
  string basename = StripProto(file->name());
  if (file->service_count() > 0) {
    GenerateHeader(basename, file, generator_context);
    GenerateSource(basename, file, generator_context);
  } else {
    // Generate an empty placeholder file for proto files
    // with no service definitions. This makes consumer build
    // logic much simpler.
    google::protobuf::scoped_ptr<ZeroCopyOutputStream> output(
        generator_context->Open(basename + ".pb.rpc.cc"));
    Printer printer(output.get(), '$');
    PrintStandardHeaders(&printer);

    printer.Print("// Auto generated empty placeholder\n");
  }

  // Handoff to the c++ generator to produce the message definitions.
  return CppGenerator::Generate(file, parameter, generator_context, error);
}
