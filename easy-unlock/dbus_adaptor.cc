// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "easy-unlock/dbus_adaptor.h"

#include <stdint.h>

#include <vector>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/file_util.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>

#include "easy-unlock/easy_unlock_service.h"

namespace easy_unlock {

namespace {

const char kBindingsPath[] =
    "/usr/share/dbus-1/interfaces/org.chromium.EasyUnlockInterface.xml";
const char kDBusIntrospectableInterface[] =
    "org.freedesktop.DBus.Introspectable";
const char kDBusIntrospectMethod[] = "Introspect";

// Utility method for extracting byte vector arguments from DBus method calls.
// |reader|: MessageReader for the method call.
// |bytes|: Byte vector extracted from message reader.
// Returns whether the bytes were successfully extracted.
bool ReadArrayOfBytes(dbus::MessageReader* reader,
                      std::vector<uint8_t>* bytes) {
  DCHECK(bytes);
  DCHECK(reader);

  const uint8_t* raw_bytes;
  size_t raw_bytes_size;
  if (!reader->PopArrayOfBytes(&raw_bytes, &raw_bytes_size))
    return false;
  bytes->assign(raw_bytes, raw_bytes + raw_bytes_size);
  return true;
}

// Reads encryption type string passed in DBus method call and converts it to
// ServiceImplementation::EncryptionType enum. Returns whether the parameter
// was successfully read and converted.
bool ReadAndConvertEncryptionType(
    dbus::MessageReader* reader,
    easy_unlock_crypto::ServiceImpl::EncryptionType* type) {
  std::string encryption_type_str;
  if (!reader->PopString(&encryption_type_str))
    return false;

  if (encryption_type_str == kEncryptionTypeNone) {
    *type = easy_unlock_crypto::ServiceImpl::ENCRYPTION_TYPE_NONE;
    return true;
  }

  if (encryption_type_str == kEncryptionTypeAES256CBC) {
    *type = easy_unlock_crypto::ServiceImpl::ENCRYPTION_TYPE_AES_256_CBC;
    return true;
  }

  return false;
}

// Reads signature type string passed in DBus method call and converts it to
// ServiceImplementation::SignatureType enum. Returns whether the parameter
// was successfully read and converted.
bool ReadAndConvertSignatureType(
    dbus::MessageReader* reader,
    easy_unlock_crypto::ServiceImpl::SignatureType* type) {
  std::string signature_type_str;
  if (!reader->PopString(&signature_type_str))
    return false;

  if (signature_type_str == kSignatureTypeECDSAP256SHA256) {
    *type = easy_unlock_crypto::ServiceImpl::SIGNATURE_TYPE_ECDSA_P256_SHA256;
    return true;
  }

  if (signature_type_str == kSignatureTypeHMACSHA256) {
    *type = easy_unlock_crypto::ServiceImpl::SIGNATURE_TYPE_HMAC_SHA256;
    return true;
  }

  return false;
}

// Utility method for running handlers for DBus method calls.
void HandleSynchronousDBusMethodCall(
    const base::Callback<
        scoped_ptr<dbus::Response>(dbus::MethodCall*)>& handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  scoped_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(response.Pass());
}

}  // namespace

DBusAdaptor::DBusAdaptor(easy_unlock::Service* service)
    : service_impl_(service) {
  CHECK(service_impl_) << "Service implementation not passed to DBus adaptor";
}

DBusAdaptor::~DBusAdaptor() {}

void DBusAdaptor::ExportDBusMethods(dbus::ExportedObject* object) {
  ExportSyncDBusMethod(object, kGenerateEcP256KeyPairMethod,
                       &DBusAdaptor::GenerateEcP256KeyPair);
  ExportSyncDBusMethod(object, kPerformECDHKeyAgreementMethod,
                       &DBusAdaptor::PerformECDHKeyAgreement);
  ExportSyncDBusMethod(object, kCreateSecureMessageMethod,
                       &DBusAdaptor::CreateSecureMessage);
  ExportSyncDBusMethod(object, kUnwrapSecureMessageMethod,
                       &DBusAdaptor::UnwrapSecureMessage);

  CHECK(object->ExportMethodAndBlock(
      kDBusIntrospectableInterface,
      kDBusIntrospectMethod,
      base::Bind(&HandleSynchronousDBusMethodCall,
                 base::Bind(&DBusAdaptor::Introspect,
                            base::Unretained(this)))));
}

scoped_ptr<dbus::Response> DBusAdaptor::Introspect(dbus::MethodCall* call) {
  std::string output;
  if (!base::ReadFileToString(base::FilePath(kBindingsPath), &output)) {
    PLOG(ERROR) << "Cannot read XML bindings from disk";
    return dbus::ErrorResponse::FromMethodCall(
        call, "Cannot read XML bindings fomr disk.", "")
        .PassAs<dbus::Response>();
  }

  scoped_ptr<dbus::Response> response(dbus::Response::FromMethodCall(call));
  dbus::MessageWriter writer(response.get());
  writer.AppendString(output);
  return response.Pass();
}

scoped_ptr<dbus::Response> DBusAdaptor::GenerateEcP256KeyPair(
    dbus::MethodCall* method_call) {
  std::vector<uint8_t> private_key;
  std::vector<uint8_t> public_key;
  service_impl_->GenerateEcP256KeyPair(&private_key, &public_key);

  scoped_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(private_key.data(), private_key.size());
  writer.AppendArrayOfBytes(public_key.data(), public_key.size());
  return response.Pass();
}

scoped_ptr<dbus::Response> DBusAdaptor::PerformECDHKeyAgreement(
    dbus::MethodCall* method_call) {
  dbus::MessageReader reader(method_call);
  std::vector<uint8_t> private_key;
  std::vector<uint8_t> public_key;
  std::vector<uint8_t> secret_key;
  if (ReadArrayOfBytes(&reader, &private_key) &&
      ReadArrayOfBytes(&reader, &public_key)) {
    secret_key =
        service_impl_->PerformECDHKeyAgreement(private_key, public_key);
  } else {
    LOG(ERROR) << "Invalid arguments for PerformECDHKeyAgreement method";
  }

  scoped_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(secret_key.data(), secret_key.size());
  return response.Pass();
}

scoped_ptr<dbus::Response> DBusAdaptor::CreateSecureMessage(
    dbus::MethodCall* method_call) {
  dbus::MessageReader reader(method_call);

  std::vector<uint8_t> payload;
  std::vector<uint8_t> key;
  std::vector<uint8_t> associated_data;
  std::vector<uint8_t> public_metadata;
  std::vector<uint8_t> verification_key_id;
  easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type;
  easy_unlock_crypto::ServiceImpl::SignatureType signature_type;

  std::vector<uint8_t> message;

  if (ReadArrayOfBytes(&reader, &payload) &&
      ReadArrayOfBytes(&reader, &key) &&
      ReadArrayOfBytes(&reader, &associated_data) &&
      ReadArrayOfBytes(&reader, &public_metadata) &&
      ReadArrayOfBytes(&reader, &verification_key_id) &&
      ReadAndConvertEncryptionType(&reader, &encryption_type) &&
      ReadAndConvertSignatureType(&reader, &signature_type)) {
    message = service_impl_->CreateSecureMessage(payload,
                                                 key,
                                                 associated_data,
                                                 public_metadata,
                                                 verification_key_id,
                                                 encryption_type,
                                                 signature_type);
  } else {
    LOG(ERROR) << "Invalid arguments for CreateSecureMessage method";
  }

  scoped_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(message.data(), message.size());
  return response.Pass();
}

scoped_ptr<dbus::Response> DBusAdaptor::UnwrapSecureMessage(
    dbus::MethodCall* method_call) {
  dbus::MessageReader reader(method_call);

  std::vector<uint8_t> message;
  std::vector<uint8_t> key;
  std::vector<uint8_t> associated_data;
  easy_unlock_crypto::ServiceImpl::EncryptionType encryption_type;
  easy_unlock_crypto::ServiceImpl::SignatureType signature_type;

  std::vector<uint8_t> unwrapped_message;

  if (ReadArrayOfBytes(&reader, &message) &&
      ReadArrayOfBytes(&reader, &key) &&
      ReadArrayOfBytes(&reader, &associated_data) &&
      ReadAndConvertEncryptionType(&reader, &encryption_type) &&
      ReadAndConvertSignatureType(&reader, &signature_type)) {
    unwrapped_message = service_impl_->UnwrapSecureMessage(message,
                                                           key,
                                                           associated_data,
                                                           encryption_type,
                                                           signature_type);
  } else {
    LOG(ERROR) << "Invalid arguments for UnwrapSecureMessage method";
  }

  scoped_ptr<dbus::Response> response =
      dbus::Response::FromMethodCall(method_call);
  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfBytes(unwrapped_message.data(), unwrapped_message.size());
  return response.Pass();
}

void DBusAdaptor::ExportSyncDBusMethod(
    dbus::ExportedObject* object,
    const std::string& method_name,
    SyncDBusMethodCallMemberFunction member) {
  DCHECK(object);
  CHECK(object->ExportMethodAndBlock(
      kEasyUnlockServiceInterface, method_name,
      base::Bind(&HandleSynchronousDBusMethodCall,
                 base::Bind(member, base::Unretained(this)))));
}

}  // namespace easy_unlock
