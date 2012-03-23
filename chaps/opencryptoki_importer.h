// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_OPENCRYPTOKI_IMPORTER_H_
#define CHAPS_OPENCRYPTOKI_IMPORTER_H_

#include "chaps/object_importer.h"

#include <string>

#include "chaps/object.h"

namespace chaps {

class ChapsFactory;
class TPMUtility;

// OpencryptokiImporter imports token objects from an opencryptoki database.
class OpencryptokiImporter : public ObjectImporter {
 public:
  OpencryptokiImporter(int slot, TPMUtility* tpm, ChapsFactory* factory);
  virtual ~OpencryptokiImporter();

  // ObjectImporter interface.
  virtual bool ImportObjects(const FilePath& path, ObjectPool* object_pool);

 private:
  // Parses an opencryptoki object file and extracts the object data and whether
  // or not it is encrypted. Returns true on success.
  bool ExtractObjectData(const std::string& object_file_content,
                         bool* is_encrypted,
                         std::string* object_data);

  // Parses an object flattened by opencryptoki.
  bool UnflattenObject(const std::string& object_data,
                       const std::string& object_name,
                       bool is_encrypted,
                       AttributeMap* attributes);

  // Determines if an object is an internal opencryptoki object and processes it
  // if so. Returns true if the object was processed.
  bool ProcessInternalObject(const AttributeMap& attributes,
                             ObjectPool* object_pool);

  // Loads the opencryptoki key hierarchy so it is available for unbinding and
  // unwrapping other objects. This can only succeed if all internal objects
  // that make up the hierarchy have been found and processed by
  // ProcessInternalObject. Typically, these objects are '00000000' through
  // '70000000' in the TOK_OBJ directory. Returns true on success.
  bool LoadKeyHierarchy();

  // Uses the TPM to decrypt the opencryptoki master key. Returns true on
  // success.
  bool DecryptMasterKey(const std::string& encrypted_master_key,
                        std::string* master_key);

  // Decrypts an object that was encrypted with the opencryptoki master key.
  // Returns true on success.
  bool DecryptObject(const std::string& key,
                     const std::string& encrypted_object_data,
                     std::string* object_data);

  // Converts all attributes of an object to Chaps format. This includes
  // unwrapping keys with the opencryptoki hierarchy and wrapping again with the
  // Chaps hierarchy. Returns true on success.
  bool ConvertToChapsFormat(AttributeMap* attributes);

  // Create an object instance complete with policies. Returns true on success.
  bool CreateObjectInstance(const AttributeMap& attributes, Object** object);

  // The token slot id. We need this to associate with our key handles.
  int slot_;
  TPMUtility* tpm_;
  ChapsFactory* factory_;
  // Opencrytoki hierarchy key handles.
  int private_root_key_;
  int private_leaf_key_;
  int public_root_key_;
  int public_leaf_key_;
  // Opencryptoki hierarchy blobs.
  std::string private_root_key_blob_;
  std::string private_leaf_key_blob_;
  std::string public_root_key_blob_;
  std::string public_leaf_key_blob_;

  DISALLOW_COPY_AND_ASSIGN(OpencryptokiImporter);
};

}  // namespace

#endif  // CHAPS_OPENCRYPTOKI_IMPORTER_H_

