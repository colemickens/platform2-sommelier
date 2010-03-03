// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cryptohome/authenticator.h"

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdlib.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "chromeos/utility.h"

namespace cryptohome {

using namespace chromeos;
using namespace file_util;
using std::string;

// system salt and user dirs start here...
const string kDefaultShadowRoot = "/home/.shadow/";

// String that appears at the start of OpenSSL cipher text with embedded salt
const string kOpenSSLMagic = "Salted__";

Authenticator::Authenticator(const string &shadow_root)
    : shadow_root_(shadow_root)
{}

Authenticator::Authenticator() : shadow_root_(kDefaultShadowRoot) {}

Authenticator::~Authenticator() {}

bool Authenticator::Init() {
  FilePath path(shadow_root_);
  return LoadFileBytes(path.Append("salt"), &system_salt_);
}

Blob Authenticator::GetSystemSalt() const {
  return system_salt_;
}

// This is the analog to cryptohome::password_to_wrapper from the
// cryptohome script.  It computes a SHA1(salt + str) and returns an
// ASCII encoded version of the result as a string.  The hashing step is
// repeated |iters| number of times.
//
string Authenticator::IteratedWrapHashedPassword(
    const FilePath &master_salt_file, const string &hashed_password,
    const int iters) const {

  string master_salt;
  if (!LoadFileString(master_salt_file, &master_salt)) {
    return false;
  }

  Blob blob(hashed_password.begin(), hashed_password.end());

  for (int i = 0; i < iters; ++i) {
    SHA_CTX ctx;
    unsigned char md_value[SHA_DIGEST_LENGTH];

    SHA1_Init(&ctx);
    SHA1_Update(&ctx, master_salt.c_str(), master_salt.length());
    SHA1_Update(&ctx, &blob.front(), blob.size());
    SHA1_Final(md_value, &ctx);

    blob.assign(md_value, md_value + SHA_DIGEST_LENGTH);
  }

  return AsciiEncode(blob);
}

string Authenticator::WrapHashedPassword(const FilePath &master_salt_file,
                                         const string &hashed_password) const {
  return IteratedWrapHashedPassword(master_salt_file, hashed_password, 1);
}

bool Authenticator::TestDecrypt(const string passphrase,
                                const Blob salt,
                                const Blob cipher_text) const {
  if (salt.size() < PKCS5_SALT_LEN) {
    LOG(ERROR) << "Invalid salt";
    return false;
  }

  unsigned char key[EVP_MAX_KEY_LENGTH], iv[EVP_MAX_IV_LENGTH];

  int rv = EVP_BytesToKey(
      EVP_aes_256_cbc(), EVP_sha1(), &salt.front(),
      reinterpret_cast<const unsigned char *>(passphrase.c_str()),
      passphrase.size(), 1, key, iv);

  if (rv != EVP_MAX_KEY_LENGTH) {
    LOG(ERROR) << "Key size is " << rv << " bytes, should be "
               << EVP_MAX_KEY_LENGTH;
    return false;
  }

  int pt_size = cipher_text.size();

  unsigned char *plain_text = new unsigned char[pt_size];
  int final_size = 0;

  EVP_CIPHER_CTX d_ctx;
  EVP_CIPHER_CTX_init(&d_ctx);
  EVP_DecryptInit_ex(&d_ctx, EVP_aes_256_ecb(), NULL, key, iv);
  rv = EVP_DecryptUpdate(&d_ctx, plain_text, &pt_size,
                         &cipher_text.front(),
                         cipher_text.size());

  rv = EVP_DecryptFinal_ex(&d_ctx, plain_text + pt_size, &final_size);

  pt_size += final_size;

  chromeos::SecureMemset(plain_text, sizeof(plain_text), 0);
  delete plain_text;

  if (rv != 1) {

    unsigned long err = ERR_get_error();
    ERR_load_ERR_strings();
    ERR_load_crypto_strings();

    LOG(INFO) << "OpenSSL Error: " << err
               << ": " << ERR_lib_error_string(err)
               << ", " << ERR_func_error_string(err)
               << ", " << ERR_reason_error_string(err);


    return false;
  }

  return true;
}

bool Authenticator::TestOneMasterKey(const FilePath &master_key_file,
                                     const string &hashed_password) const {
  if (system_salt_.empty()) {
    LOG(ERROR) << "System salt not loaded.";
    return false;
  }

  Blob cipher_text;
  if (!LoadFileBytes(master_key_file, &cipher_text)) {
    LOG(ERROR) << "Error loading master key from '"
               << master_key_file.value() << "'";
    return false;
  }

  unsigned int header_size = kOpenSSLMagic.length() + PKCS5_SALT_LEN;
  if (cipher_text.size() <= header_size) {
    LOG(ERROR) << "Master key file too short: '"
               << master_key_file.value() << "'";
    return false;
  }

  string magic(cipher_text.begin(),
               cipher_text.begin() + kOpenSSLMagic.length());
  if (magic != kOpenSSLMagic) {
    LOG(ERROR) << "Invalid magic in master key file: '"
               << master_key_file.value() << "'";
    return false;
  }

  Blob salt(cipher_text.begin() + kOpenSSLMagic.length(),
            cipher_text.begin() + header_size);

  string passphrase =
      WrapHashedPassword(FilePath(master_key_file.value() + ".salt"),
                         hashed_password);

  cipher_text.erase(cipher_text.begin(), cipher_text.begin() + header_size);
  return TestDecrypt(passphrase, salt, cipher_text);
}

bool Authenticator::TestAllMasterKeys(const Credentials &credentials) const {
#ifdef CHROMEOS_PAM_LOCALACCOUNT
  if (credentials.IsLocalAccount()) {
    LOG(WARNING) << "Logging in with local account credentials.";
    return true;
  }
#endif

  if (system_salt_.empty()) {
    LOG(ERROR) << "System salt not loaded.";
    return false;
  }

  FilePath shadow_root(shadow_root_);
  FilePath user_path =
      shadow_root.Append(credentials.GetObfuscatedUsername(system_salt_));
  string weak_hash(credentials.GetPasswordWeakHash(system_salt_));
  char index_str[5];

  // Test against all of the master keys (master.0, master.1, ...)
  for (int i = 0; /* loop forever */ ; ++i) {
    string filename("master.");
    if (0 == snprintf(index_str, sizeof(index_str), "%i", i))
      return false;
    filename.append(index_str);

    FilePath master_key_file = user_path.Append(filename);

    if (!AbsolutePath(&master_key_file)) {
      // master.N can't be read, assume we're done and have failed
      break;
    }

    if (TestOneMasterKey(master_key_file, weak_hash)) {
      // decrypted ok, return success
      return true;
    }
  }

  return false;
}

bool Authenticator::LoadFileBytes(const FilePath &path, Blob *blob) const {
  CHECK(blob);
  int64 file_size;
  if (!PathExists(path)) {
    LOG(ERROR) << path.value() << " does not exist!";
    return false;
  }
  if (!GetFileSize(path, &file_size)) {
    LOG(ERROR) << "Could not get size of " << path.value();
    return false;
  }
  char buf[file_size];
  int data_read = ReadFile(path, buf, file_size);
  blob->assign(buf, buf + data_read);
  return true;
}

bool Authenticator::LoadFileString(const FilePath &path, string *str) const {
  return PathExists(path) && ReadFileToString(path, str);
}

}  // namespace cryptohome
