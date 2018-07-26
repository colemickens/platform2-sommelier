// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <stdio.h>

#include <openssl/ec.h>
#include <openssl/pem.h>

// We use a P256 curve.
static const int OOBE_AUTOCONFIG_CURVE_ID = NID_X9_62_prime256v1;

static void GenerateKeyPair(void) {
  EC_KEY* key = EC_KEY_new_by_curve_name(OOBE_AUTOCONFIG_CURVE_ID);
  assert(key != NULL);
  assert(EC_KEY_generate_key(key));

  EC_KEY_set_asn1_flag(key, OPENSSL_EC_NAMED_CURVE);
  PEM_write_ECPrivateKey(stdout, key, NULL, NULL, 0, 0, NULL);
  PEM_write_EC_PUBKEY(stdout, key);

  EC_KEY_free(key);
}

static void ShowHelpAndExit(void) {
  fprintf(stderr,
      "cros_oobe_crypto\n"
      "\n"
      "\tGenerates a prime256v1 key pair for OOBE autoconfiguration signing.\n"
      "\n"
      "\tThe private key is printed in PEM format on the first 5 lines of\n"
      "\toutput. The public key printed out in PEM format on the following 4\n"
      "\tlines.\n"
      "\n"
      "\tEach invocation of `cros_oobe_crypto` will create a new keypair.\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
  if (argc > 1) {
    ShowHelpAndExit();
  }

  GenerateKeyPair();
  return 0;
}
