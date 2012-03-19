// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inst_util.h"

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <linux/fs.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

extern "C" {
#include "vboot/dump_kernel_config.h"
}
#include "vboot/kernel_blob.h"

using std::string;


string StringPrintf(const string& format, ...) {
  va_list ap;

  va_start(ap, format);
  int v_result = vsnprintf(NULL,
                           0,
                           format.c_str(),
                           ap);
  va_end(ap);

  if (v_result < 0) {
    printf("Error in SpringPrintf - sizing\n");
    return "";
  }

  const int size = v_result + 1;

  char* sprintf_buffer = reinterpret_cast<char *>(malloc(size));

  if (!sprintf_buffer) {
    printf("Error in SpringPrintf - memory allocation\n");
    return "";
  }

  va_start(ap, format);
  v_result = vsnprintf(sprintf_buffer,
                       size,
                       format.c_str(),
                       ap);
  va_end(ap);

  if (v_result < 0 || v_result >= size) {
    free(sprintf_buffer);
    printf("Error in SpringPrintf - formatting\n");
    return "";
  }

  string result(sprintf_buffer);
  free(sprintf_buffer);
  return result;
}

void SplitString(const string& str,
                 char split,
                 std::vector<string>* output) {
  output->clear();

  size_t i = 0;
  while (true) {
    size_t split_at = str.find(split, i);
    if (split_at == str.npos)
      break;
    output->push_back(str.substr(i, split_at-i));
    i = split_at + 1;
  }

  output->push_back(str.substr(i));
}

// This is a place holder to invoke the backing scripts. Once all scripts have
// been rewritten as library calls this command should be deleted.
// If you are passing more than one command in cmdoptions you need it to be
// space separated.
int RunCommand(const string& command) {
  printf("Command: %s\n", command.c_str());

  int result = system(command.c_str());

  if (result != 0)
    printf("Failed Command: %s - %d\n", command.c_str(), result);

  return result;
}

string LsbReleaseValue(const string& file,
                       const string& key) {
  string preamble = key + "=";

  string line;
  std::ifstream lsb_file(file.c_str());

  if (!lsb_file.is_open())
    return "";

  while (lsb_file.good()) {
    line = "";
    getline(lsb_file, line);
    if (line.compare(0, preamble.size(), preamble) == 0)
      return line.substr(preamble.size());
  }
  lsb_file.close();

  return "";
}

// If less is a lower version number than right
bool VersionLess(const string& left,
                 const string& right) {
  std::vector<string> left_parts;
  std::vector<string> right_parts;

  SplitString(left, '.', &left_parts);
  SplitString(right, '.', &right_parts);

  // We changed from 3 part versions to 4 part versions.
  // 3 part versions are always newer than 4 part versions
  if (left_parts.size() == 3 && right_parts.size() == 4)
    return false;

  if (left_parts.size() == 4 && right_parts.size() == 3)
    return true;

  // There should be no other way for the lengths to be different
  //assert(left_parts.length() == right_parts.length());

  for (unsigned int i = 0; i < left_parts.size(); i++) {
    int left_value = atoi(left_parts[i].c_str());
    int right_value = atoi(right_parts[i].c_str());

    if (left_value < right_value)
      return true;

    if (left_value > right_value)
      return false;
  }

  // They are equal, and thus not less than.
  return false;
}

const std::string mmcblk_dev = "/dev/mmcblk";

string GetBlockDevFromPartitionDev(const string& partition_dev) {
  size_t i = partition_dev.length();

  while (i > 0 && isdigit(partition_dev[i-1]))
    i--;

  // mmcblk devices are of the form "/dev/mmcblk12p34"
  if (partition_dev.compare(0, mmcblk_dev.size(), mmcblk_dev) == 0) {
    // If it ends with a p, strip off the p. If it doesn't there was
    // no partition at the end (/dev/mmcblk12) return unmodified.
    if (partition_dev[i-1] == 'p')
      i--;
    else
      return partition_dev;
  }

  return partition_dev.substr(0, i);
}

int GetPartitionFromPartitionDev(const string& partition_dev) {
  size_t i = partition_dev.length();

  while (i > 0 && isdigit(partition_dev[i-1]))
    i--;

  // mmcblk devices are of the form "/dev/mmcblk12p34"
  // If there is no ending p, There was no partition at the end (/dev/mmcblk12)
  if ((partition_dev.compare(0, mmcblk_dev.size(), mmcblk_dev) == 0) &&
      (partition_dev[i-1] != 'p')) {
    return 0;
  }

  string partition_str = partition_dev.substr(i, i+1);

  int result = atoi(partition_str.c_str());

  if (result == 0)
    printf("Bad partiion number from '%s'\n", partition_dev.c_str());

  return result;
}

string MakePartitionDev(const string& block_dev, int partition) {
  if (block_dev.compare(0, mmcblk_dev.size(), mmcblk_dev) == 0)
    return StringPrintf("%sp%d", block_dev.c_str(), partition);

  return StringPrintf("%s%d", block_dev.c_str(), partition);
}

// Convert /blah/file to /blah
string Dirname(const string& filename) {
  size_t last_slash = filename.rfind('/');

  if (last_slash == string::npos)
    return "";

  return filename.substr(0, last_slash);
}

// rm *pack from /dirname
bool RemovePackFiles(const string& dirname) {
  DIR *dp;
  struct dirent *ep;

  dp = opendir(dirname.c_str());

  if (dp == NULL)
    return false;

  while ( (ep = readdir(dp)) ) {
    string filename = ep->d_name;

    // Skip . files
    if (filename.compare(0, 1, ".") == 0)
      continue;

    if ((filename.size() < 4) ||
        (filename.compare(filename.size() - 4, 4, "pack") != 0))
      continue;

    string full_filename = dirname + '/' + filename;

    printf("Unlinked file %s\n", full_filename.c_str());
    unlink(full_filename.c_str());
  }

  closedir (dp);

  return true;
}

bool Touch(const string& filename) {
  int fd = open(filename.c_str(),
                O_WRONLY | O_CREAT,
                S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

  if (fd == -1)
    return false;

  return (close(fd) == 0);
}

bool R10FileSystemPatch(const string& dev_name) {
  // See bug chromium-os:11517. This fixes an old FS corruption problem.
  const int offset = 1400;

  int fd = open(dev_name.c_str(), O_WRONLY);

  if (fd == -1) {
    printf("Failed to open\n");
    return false;
  }

  // Write out stuff
  if (lseek(fd, offset, SEEK_SET) != offset) {
    printf("Failed to seek\n");
    return false;
  }

  char buff[] = { 0, 0 };

  if (write(fd, buff, sizeof(buff)) != 2) {
    printf("Failed to write\n");
    return false;
  }

  return (close(fd) == 0);
}

bool MakeFileSystemRw(const string& dev_name, bool rw) {
  const int offset = 0x464 + 3; // Set 'highest' byte

  int fd = open(dev_name.c_str(), O_WRONLY);

  if (fd == -1) {
    printf("Failed to open\n");
    return false;
  }

  // Write out stuff
  if (lseek(fd, offset, SEEK_SET) != offset) {
    printf("Failed to seek\n");
    return false;
  }

  // buff[0] is disable_rw_mount, buff[1] is rw enabled
  unsigned char buff[] = { 0xFF , 0 };

  if (write(fd, &(buff[rw]), 1) != 1) {
    printf("Failed to write\n");
    return false;
  }

  return (close(fd) == 0);
}

// hdparm -r 1 /device
bool MakeDeviceReadOnly(const string& dev_name) {
  int fd = open(dev_name.c_str(), O_RDONLY|O_NONBLOCK);
  if (fd == -1)
    return false;

  int readonly = 1;

  bool result = ioctl(fd, BLKROSET, &readonly) == 0;

  close(fd);

  return result;
}

extern "C" {

// The external dumpkernelconfig.a library depends on this symbol
// existing, so I redefined it here. I deserve to suffer
// very, very painfully for this, but hey.
void VbExError(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  fprintf(stderr, "ERROR: ");
  va_end(ap);
}

}

string DumpKernelConfig(const string& kernel_dev) {
  string result;

  // Map the kernel image blob.
  size_t blob_size;
  uint8_t* blob = static_cast<uint8_t*>(MapFile(kernel_dev.c_str(),
                                                &blob_size));

  if (!blob) {
    printf("Error reading input file\n");
    return result;
  }

  uint8_t *config = find_kernel_config(blob,
                                       static_cast<uint64_t>(blob_size),
                                       CROS_32BIT_ENTRY_ADDR);
  if (!config) {
    printf("Error parsing input file\n");
    munmap(blob, blob_size);
    return result;
  }

  result = string(reinterpret_cast<const char *>(config),
                  static_cast<size_t>(CROS_CONFIG_SIZE));
  munmap(blob, blob_size);

  return result;
}

string ExtractKernelArg(const string& kernel_config,
                        const string& key) {

  // We are really looking for key=value
  string preamble = key + "=";

  size_t i;

  // Search for arg...
  for (i = 0; i < kernel_config.size(); i++) {

    // If we hit a " while searching, skip to matching quote
    if (kernel_config[i] == '"') {
      i++;
      while (i < kernel_config.size() && kernel_config[i] != '"')
        i++;
    }

    // if we found the key
    if (kernel_config.compare(i, preamble.size(), preamble) == 0)
      break;
  }

  // Jump past the key
  i += preamble.size();

  // Didn't find the key
  if (i >= kernel_config.size())
    return "";

  char closing_char = ' ';

  // If it's a quoted value, look for closing quote
  if (kernel_config[i] == '"') {
    i++;
    closing_char = '"';
  }

  size_t value_start = i;

  while (i < kernel_config.size() && kernel_config[i] != closing_char)
    i++;

  return kernel_config.substr(value_start, i - value_start);
}
