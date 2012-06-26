// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <syslog.h>
#include <assert.h>
#include <zlib.h>
#include <curl/curl.h>
#include "common.h"
#include "uploader.h"

static int zip(FILE* in, FILE* out, int level);

Uploader::Uploader(const std::string& input_data_file,
                   const std::string& board,
                   const std::string& chromeos_version,
                   const std::string& server_url) :
  input_data_file_(input_data_file),
  output_data_file_(input_data_file+COMPRESSED_EXTENSION),
  board_(board),
  chromeos_version_(chromeos_version),
  server_url_(server_url) {}

int Uploader::CompressAndUpload() {
  // First, try gzipping.
  int return_code = DoGzip();
  if (return_code != QUIPPER_SUCCESS) {
    // Couldn't gzip, so remove profile data.
    remove(input_data_file_.c_str());
  }
  return_code = DoUpload();
  if (return_code != QUIPPER_SUCCESS) {
    // Couldn't upload, so remove profile + compressed.
    remove(input_data_file_.c_str());
    remove(output_data_file_.c_str());
  }
  return return_code;
}

int Uploader::DoGzip() {
  // Create and open files.
  FILE* input = fopen(input_data_file_.c_str(), "r");
  if (input == NULL) {
    syslog(LOG_NOTICE, "Could not open %s\n", input_data_file_.c_str());
    return QUIPPER_FAIL;
  }
  FILE* output = fopen(output_data_file_.c_str(), "w");
  // Gzip input to output file.
  int ret = zip(input, output, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) {
    syslog(LOG_NOTICE, "Could not zip, return=%d\n", ret);
    fclose(input);
    fclose(output);
    return QUIPPER_FAIL;
  }
  fclose(input);
  fclose(output);
  return QUIPPER_SUCCESS;
}

int Uploader::DoUpload() {
  CURL* curl;
  CURLcode res;
  int ret;
  struct curl_httppost* formpost = NULL;
  struct curl_httppost* lastptr = NULL;
  struct curl_slist* headerlist = NULL;
  const char buf[] = "Expect:";
  curl_global_init(CURL_GLOBAL_ALL);
  // Set up multipart-form parameters.
  curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "profile_data",
               CURLFORM_FILE, output_data_file_.c_str(), CURLFORM_END);

  curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "board",
               CURLFORM_COPYCONTENTS, board_.c_str(), CURLFORM_END);

  curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "chromeos_version",
               CURLFORM_COPYCONTENTS, chromeos_version_.c_str(), CURLFORM_END);

  curl = curl_easy_init();
  headerlist = curl_slist_append(headerlist, buf);
  // Execute the POST
  if (curl) {
    curl_easy_setopt(curl, CURLOPT_URL, server_url_.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
      syslog(LOG_NOTICE, "Curl Error: %s\n", curl_easy_strerror(res));
      ret = QUIPPER_FAIL;
    } else {
      ret = QUIPPER_SUCCESS;
    }
  } else {
    // No curl.
    syslog(LOG_NOTICE, "Could not curl_easy_init().\n");
    ret = QUIPPER_FAIL;
  }
  curl_easy_cleanup(curl);
  curl_formfree(formpost);
  curl_slist_free_all(headerlist);
  return ret;
}

// Code taken from zpipe.c
static int zip(FILE* source, FILE* dest, int level) {
  int ret, flush;
  unsigned have;
  z_stream strm;
  unsigned char in[CHUNK];
  unsigned char out[CHUNK];
  assert(source != NULL);
  assert(dest != NULL);

  // Allocate deflate state.
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  ret = deflateInit2(&strm, level, Z_DEFLATED, (15+16), 8, Z_DEFAULT_STRATEGY);
  if (ret != Z_OK) {
    return ret;
  }

  // Compress until end of file.
  do {
    strm.avail_in = fread(in, 1, CHUNK, source);
    if (ferror(source)) {
      (void)deflateEnd(&strm);
      return Z_ERRNO;
    }
    flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
    strm.next_in = in;

    // Run deflate() on input until output buffer full.
    // Finish compression if all of source has been read in.
    do {
      strm.avail_out = CHUNK;
      strm.next_out = out;
      ret = deflate(&strm, flush);
      assert(ret != Z_STREAM_ERROR);
      have = CHUNK - strm.avail_out;
      if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
        (void)deflateEnd(&strm);
        return Z_ERRNO;
      }
    } while (strm.avail_out == 0);
    assert(strm.avail_in == 0);
    // Done when last data in file is processed.
  } while (flush != Z_FINISH);
  assert(ret == Z_STREAM_END);
  // Clean up and return
  (void)deflateEnd(&strm);
  return Z_OK;
}
