// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libipproto/ipp_base.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "libipproto/ipp_protocol.h"

namespace ipp {

namespace {

// Sets attributes-charset and attributes-natural-language.
void SetDefaultPackageAttributes(Package* package) {
  Group* oper_grp = package->GetGroup(GroupTag::operation_attributes);
  Collection* coll_grp = nullptr;
  if (oper_grp != nullptr &&
      (coll_grp = oper_grp->GetCollection()) != nullptr) {
    Attribute* attr_charset =
        coll_grp->GetAttribute(AttrName::attributes_charset);
    Attribute* attr_language =
        coll_grp->GetAttribute(AttrName::attributes_natural_language);
    if (attr_charset != nullptr && attr_charset->GetState() == AttrState::unset)
      attr_charset->SetValue(std::string("utf-8"));
    if (attr_language != nullptr &&
        attr_language->GetState() == AttrState::unset)
      attr_language->SetValue(std::string("en-us"));
  }
}

// Sets status-message basing on status-code.
void SetDefaultResponseAttributes(Response* package) {
  Group* oper_grp = package->GetGroup(GroupTag::operation_attributes);
  Collection* coll_grp = nullptr;
  if (oper_grp != nullptr &&
      (coll_grp = oper_grp->GetCollection()) != nullptr) {
    Attribute* attr = coll_grp->GetAttribute(AttrName::status_message);
    if (attr != nullptr && attr->GetState() == AttrState::unset)
      attr->SetValue(ipp::ToString(package->StatusCode()));
  }
}

// Clear all values in the package.
void ClearPackage(Package* package) {
  const std::vector<Group*> groups = package->GetAllGroups();
  for (auto g : groups) {
    if (g->IsASet()) {
      g->Resize(0);
    } else {
      g->GetCollection()->ResetAllAttributes();
    }
  }
}

Version GetVersion(const Protocol* protocol) {
  uint16_t v = protocol->major_version_number_;
  v <<= 8;
  v += protocol->minor_version_number_;
  return static_cast<Version>(v);
}

void SetVersion(Protocol* protocol, Version version) {
  uint16_t v = static_cast<uint16_t>(version);
  protocol->major_version_number_ = (v >> 8);
  protocol->minor_version_number_ = (v & 0xff);
}

}  // namespace

Client::Client(Version version, int32_t request_id)
    : protocol_(std::make_unique<Protocol>()) {
  SetVersionNumber(version);
  protocol_->request_id_ = request_id;
}

// Destructor must be defined here, because we do not have definition of
// Protocol in the header.
Client::~Client() = default;

Version Client::GetVersionNumber() const {
  return GetVersion(protocol_.get());
}

void Client::SetVersionNumber(Version version) {
  SetVersion(protocol_.get(), version);
}

void Client::BuildRequestFrom(Request* request) {
  protocol_->ResetContent();
  SetDefaultPackageAttributes(request);
  ++(protocol_->request_id_);
  protocol_->operation_id_or_status_code_ =
      static_cast<uint16_t>(request->GetOperationId());
  protocol_->LoadFromPackage(request);
}

bool Client::WriteRequestFrameTo(std::vector<uint8_t>* data) const {
  if (data == nullptr)
    return false;
  data->resize(GetFrameLength());
  return protocol_->WriteToFrame(data->data());
}

std::size_t Client::GetFrameLength() const {
  return protocol_->GetFrameLength();
}

bool Client::ReadResponseFrameFrom(const uint8_t* ptr,
                                   const uint8_t* const buf_end) {
  protocol_->ResetContent();
  return protocol_->ReadFromFrame(ptr, buf_end);
}

bool Client::ReadResponseFrameFrom(const std::vector<uint8_t>& data) {
  protocol_->ResetContent();
  const uint8_t* ptr = (data.empty()) ? (nullptr) : (&(data[0]));
  return protocol_->ReadFromFrame(ptr, ptr + data.size());
}

bool Client::ParseResponseAndSaveTo(Response* response,
                                    bool log_unknown_values) {
  ClearPackage(response);
  response->StatusCode() =
      static_cast<Status>(protocol_->operation_id_or_status_code_);
  return protocol_->SaveToPackage(response, log_unknown_values);
}

const std::vector<Log>& Client::GetErrorLog() const {
  return protocol_->errors_;
}

Server::Server(Version version, int32_t request_id)
    : protocol_(std::make_unique<Protocol>()) {
  SetVersionNumber(version);
  protocol_->request_id_ = request_id;
}

// Destructor must be defined here, because we do not have definition of
// Protocol in the header.
Server::~Server() = default;

Version Server::GetVersionNumber() const {
  return GetVersion(protocol_.get());
}

void Server::SetVersionNumber(Version version) {
  SetVersion(protocol_.get(), version);
}

bool Server::ReadRequestFrameFrom(const uint8_t* ptr,
                                  const uint8_t* const buf_end) {
  protocol_->ResetContent();
  return protocol_->ReadFromFrame(ptr, buf_end);
}

bool Server::ReadRequestFrameFrom(const std::vector<uint8_t>& data) {
  protocol_->ResetContent();
  return protocol_->ReadFromFrame(data.data(), data.data() + data.size());
}

Operation Server::GetOperationId() const {
  return static_cast<Operation>(protocol_->operation_id_or_status_code_);
}

bool Server::ParseRequestAndSaveTo(Request* request, bool log_unknown_values) {
  ClearPackage(request);
  return protocol_->SaveToPackage(request, log_unknown_values);
}

void Server::BuildResponseFrom(Response* package) {
  protocol_->ResetContent();
  SetDefaultPackageAttributes(package);
  SetDefaultResponseAttributes(package);
  protocol_->operation_id_or_status_code_ =
      static_cast<uint16_t>(package->StatusCode());
  protocol_->LoadFromPackage(package);
}

std::size_t Server::GetFrameLength() const {
  return protocol_->GetFrameLength();
}

bool Server::WriteResponseFrameTo(std::vector<uint8_t>* data) const {
  if (data == nullptr)
    return false;
  data->resize(GetFrameLength());
  return protocol_->WriteToFrame(data->data());
}

const std::vector<Log>& Server::GetErrorLog() const {
  return protocol_->errors_;
}

}  // namespace ipp
