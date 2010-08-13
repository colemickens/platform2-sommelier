// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"

#include <string>

class DictionaryValue;

namespace login_manager {

// Signed preferences store.  Keeps track of a dictionary of arbitrary key/value
// pairs, and also a whitelist of usernames.  Every value stored here has an
// associated digital signature.  This class does no signature checking; it is
// up to the caller to verify signatures before adding items to the store, and
// after extracting them from the store.
class PrefStore {
 public:
  explicit PrefStore(const FilePath& prefs_path);
  virtual ~PrefStore();

  // Populate |prefs_| with the file at |prefs_path_|.  Returns true on success.
  // If the file does not exist, create |prefs_| anew and return true.
  // If the file can't be loaded, create |prefs_| anew and return false.
  // This is so that we can continue to function while, at least, returning
  // some indication that bad things have happeend.
  virtual bool LoadOrCreate();

  // Persists the contents of |prefs_| to disk at |prefs_path_|.
  virtual bool Persist();

  // Add |name| to the whitelist, associated with the Owner's |signature|.
  // |name| should be an email address.
  virtual void Whitelist(const std::string& name, const std::string& signature);

  // Remove |name| and associated signature from the whitelist.
  // |name| should be an email address.
  virtual void Unwhitelist(const std::string& name);

  // Get the signature associated with |name| from the whitelist.  Returns true
  // if |name| is found, false otherwise.
  // |name| should be an email address.
  virtual bool GetFromWhitelist(const std::string& name,
                                std::string* signature_out);

  // Set |name| = |value|, associating them with |signature|.
  // Creates the preference if it didn't exist.
  //
  // It is an error to pass in a |name| with embedded '.' characters.
  virtual void Set(const std::string& name,
                   const std::string& value,
                   const std::string& signature);

  // Get the value and signature associated with the key |name|.  Pass them
  // back in |value_out| and |signature_out|, which must not be NULL.
  // Returns true if the key is found, false otherwise.
  //
  // It is an error to pass in a |name| with embedded '.' characters.
  virtual bool Get(const std::string& name,
                   std::string* value_out,
                   std::string* signature_out);

  // Get the value and signature associated with the key |name|, and
  // then delete this record from the store.  Returns true if the key
  // is found, and the values are passed back in |value_out| and
  // |signature_out|.  These must not be NULL.
  // Returns false on failure.
  //
  // It is an error to pass in a |name| with embedded '.' characters.
  virtual bool Remove(const std::string& name,
                       std::string* value_out,
                       std::string* signature_out);

  // Removes the record for |name| from the store.  If it didn't exist...who
  // cares, you just wanted it gone :-)
  //
  // It is an error to pass in a |name| with embedded '.' characters.
  virtual void Delete(const std::string& name);

  static const char kWhitelistPrefix[];
  static const char kValueField[];
  static const char kSignatureField[];
  static const char kDefaultPath[];

 private:
  base::JSONReader reader_;
  scoped_ptr<DictionaryValue> prefs_;
  const FilePath prefs_path_;

  // Remove the record for |key| from |prefs_| and return in |out|.
  // Returns false upon failure.
  bool RemoveOneString(const std::string& key, std::string* out);
};

}  // namespace login_manager
