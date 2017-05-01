// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <signal.h>

#include <base/memory/ptr_util.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "helper_process.h"
#include "imageloader.h"
#include "imageloader_impl.h"
#include "mount_helper.h"

namespace {

constexpr uint8_t kProdPublicKey[] = {
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x53, 0xd9, 0x6f, 0xb1, 0x92, 0x97, 0x39, 0xa9, 0x97,
    0x18, 0xbe, 0xa7, 0x97, 0x15, 0x06, 0x27, 0x9c, 0x55, 0xa5, 0x40, 0xc1,
    0x0f, 0x98, 0xfa, 0xd8, 0x61, 0x18, 0xee, 0xcf, 0xf3, 0xbb, 0xf9, 0x6e,
    0x6d, 0xa0, 0x66, 0xd2, 0x29, 0xf0, 0x78, 0x5b, 0x7a, 0xab, 0x54, 0xca,
    0x53, 0x16, 0xb0, 0xf9, 0xc4, 0xd8, 0x1d, 0x93, 0x5b, 0x83, 0x6e, 0xa5,
    0x65, 0xe5, 0x71, 0xbc, 0x8d, 0x72, 0x02};

// The path where the components are stored on the device.
constexpr char kComponentsPath[] = "/var/lib/imageloader";
// The base path where the components are mounted.
constexpr char kMountPath[] = "/run/imageloader";
// The location of the container public key.
constexpr char kContainerPublicKeyPath[] =
    "/usr/share/misc/oci-container-key-pub.pem";

struct ScopedPubkeyCloser {
  inline void operator()(EVP_PKEY* pubkey) {
    if (pubkey)
      EVP_PKEY_free(pubkey);
  }
};
using ScopedPubkey = std::unique_ptr<EVP_PKEY, ScopedPubkeyCloser>;

bool LoadKeyFromFile(const std::string& file, std::vector<uint8_t>* key_out) {
  CHECK(key_out);

  base::ScopedFILE key_file(fopen(file.c_str(), "r"));
  if (!key_file) {
    LOG(WARNING) << "Could not find key file " << file;
    return false;
  }

  ScopedPubkey pubkey(
      PEM_read_PUBKEY(key_file.get(), nullptr, nullptr, nullptr));
  if (!pubkey) {
    LOG(WARNING) << "Key file " << file << " was not a public key";
    return false;
  }

  uint8_t *der_key = nullptr;
  int der_len = i2d_PUBKEY(pubkey.get(), &der_key);
  if (der_len < 0) {
    LOG(WARNING) << "Failed to export public key in DER format";
    return false;
  }

  key_out->clear();
  key_out->insert(key_out->begin(), der_key, der_key + der_len);
  OPENSSL_free(der_key);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  DEFINE_bool(mount, false,
              "Rather than starting a dbus daemon, verify and mount a single "
              "component and exit immediately.");
  DEFINE_string(mount_component, "",
                "Specifies the name of the component when using --mount.");
  DEFINE_string(mount_point, "",
                "Specifies the mountpoint when using --mount.");
  DEFINE_int32(mount_helper_fd, -1,
               "Control socket for starting an ImageLoader subprocess. Used "
               "internally.");
  brillo::FlagHelper::Init(argc, argv, "imageloader");

  brillo::OpenLog("imageloader", true);
  brillo::InitLog(brillo::kLogToSyslog);

  // Executing this as the helper process if specified.
  if (FLAGS_mount_helper_fd >= 0) {
    CHECK_GT(FLAGS_mount_helper_fd, -1);
    base::ScopedFD fd(FLAGS_mount_helper_fd);
    imageloader::MountHelper mount_helper(std::move(fd));
    return mount_helper.Run();
  }

  imageloader::Keys keys;
  // The order of key addition below is important.
  // 1. Prod key, used to sign Flash.
  keys.push_back(std::vector<uint8_t>(std::begin(kProdPublicKey),
                                      std::end(kProdPublicKey)));
  // 2. Container key.
  std::vector<uint8_t> container_key;
  if (LoadKeyFromFile(kContainerPublicKeyPath, &container_key))
    keys.push_back(container_key);

  imageloader::ImageLoaderConfig config(keys, kComponentsPath, kMountPath);
  auto helper_process = base::MakeUnique<imageloader::HelperProcess>();
  helper_process->Start(argc, argv, "--mount_helper_fd");

  // Load and mount the specified component and exit.
  if (FLAGS_mount) {
    // Run with minimal privilege.
    imageloader::ImageLoader::EnterSandbox();

    if (FLAGS_mount_point == "" || FLAGS_mount_component == "") {
      LOG(ERROR) << "--mount_component=name and --mount_point=path must be set "
                    "with --mount";
      return 1;
    }
    // Access the ImageLoaderImpl directly to avoid needless dbus dependencies,
    // which may not be available at early boot.
    imageloader::ImageLoaderImpl loader(std::move(config));
    if (!loader.LoadComponent(FLAGS_mount_component, FLAGS_mount_point,
                              helper_process.get())) {
      LOG(ERROR) << "Failed to verify and mount component.";
      return 1;
    }
    return 0;
  }

  // Run as a daemon and wait for dbus requests.
  imageloader::ImageLoader daemon(std::move(config), std::move(helper_process));
  daemon.Run();

  return 0;
}
