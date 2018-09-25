// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "oobe_config/utils.h"

#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <brillo/process.h>
#include <crypto/scoped_openssl_types.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

using base::FilePath;
using std::string;
using std::vector;

namespace oobe_config {

int RunCommand(const vector<string>& command) {
  LOG(INFO) << "Command: " << base::JoinString(command, " ");

  brillo::ProcessImpl proc;
  for (const auto& arg : command) {
    proc.AddArg(arg);
  }
  return proc.Run();
}

bool SignFile(const FilePath& priv_key,
              const FilePath& src,
              const FilePath& dst) {
  // Reading the source file.
  string src_content;
  if (!base::ReadFileToString(src, &src_content)) {
    PLOG(ERROR) << "Failed to read the source file " << src.value();
    return false;
  }

  // Reading the private key.
  base::ScopedFILE pkf(base::OpenFile(priv_key, "r"));
  if (!pkf) {
    PLOG(ERROR) << "Failed to open the private key file " << priv_key.value();
    return false;
  }
  crypto::ScopedEVP_PKEY private_key(
      PEM_read_PrivateKey(pkf.get(), nullptr, nullptr, nullptr));
  if (!private_key) {
    PLOG(ERROR) << "Failed to read the PEM private key file "
                << priv_key.value();
    return false;
  }

  // Creating the context.
  crypto::ScopedEVP_MD_CTX mdctx(EVP_MD_CTX_create());
  if (!mdctx) {
    LOG(ERROR) << "Failed to create a EVP_MD context.";
    return false;
  }

  // Signing the |src| file.
  size_t signature_len = 0;
  if (EVP_DigestSignInit(mdctx.get(), nullptr, EVP_sha256(), nullptr,
                         private_key.get()) != 1 ||
      EVP_DigestSignUpdate(mdctx.get(), src_content.c_str(),
                           src_content.length()) != 1 ||
      EVP_DigestSignFinal(mdctx.get(), nullptr, &signature_len) != 1) {
    LOG(ERROR) << "Failed to sign the source file " << src.value();
  }
  // Now we know the size of the signature, let's get it.
  crypto::ScopedOpenSSLBytes signature(reinterpret_cast<unsigned char*>(
      OPENSSL_malloc(sizeof(unsigned char) * signature_len)));
  if (!signature) {
    PLOG(ERROR) << "Failed to malloc " << signature_len
                << " bytes for the signature.";
    return false;
  }
  if (EVP_DigestSignFinal(mdctx.get(), signature.get(), &signature_len) != 1) {
    LOG(ERROR) << "Getting the signature failed; however, technically this"
               << " should not happen!";
    return false;
  }
  if (!base::WriteFile(dst, reinterpret_cast<char*>(signature.get()),
                       signature_len)) {
    PLOG(ERROR) << "Failed to write the signature to file " << dst.value();
    return false;
  }

  return true;
}

}  // namespace oobe_config
