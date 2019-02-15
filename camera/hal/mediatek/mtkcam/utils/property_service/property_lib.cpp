/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <property_lib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#define PROPERTY_STORAGE "/run/camera/property_storage"

void update_property(const char* prop_name, char* prop_value) {
  FILE* fp;
  char property[PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX];
  int find = 0;
  int64_t pos = 0;

  fp = fopen(PROPERTY_STORAGE, "rb+");
  if (!fp) {
    fp = fopen(PROPERTY_STORAGE, "wb+");

    if (!fp) {
      return;
    }
  }

  flock(fileno(fp), LOCK_EX);

  while (fread(property, PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX, 1, fp)) {
    if (strcmp(property, prop_name) == 0) {
      find = 1;
      snprintf(property + PROPERTY_KEY_MAX, PROPERTY_VALUE_MAX, "%s",
               prop_value);
      fseek(fp, pos, SEEK_SET);
      fwrite(property, PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX, 1, fp);
      break;
    }
    pos += PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX;
  }

  if (!find) {
    snprintf(property, PROPERTY_KEY_MAX, "%s", prop_name);
    snprintf(property + PROPERTY_KEY_MAX, PROPERTY_VALUE_MAX, "%s", prop_value);
    fseek(fp, pos, SEEK_SET);
    fwrite(property, PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX, 1, fp);
  }

  fclose(fp);
}

int fetch_property(const char* prop_name, char* prop_value) {
  FILE* fp;
  char property[PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX];
  int find = 0;

  fp = fopen(PROPERTY_STORAGE, "rb");
  if (!fp) {
    return 0;
  }

  flock(fileno(fp), LOCK_SH);

  while (fread(property, PROPERTY_KEY_MAX + PROPERTY_VALUE_MAX, 1, fp)) {
    if (strcmp(property, prop_name) == 0) {
      find = 1;
      snprintf(prop_value, PROPERTY_VALUE_MAX, "%s",
               property + PROPERTY_KEY_MAX);
      break;
    }
  }

  fclose(fp);

  return find;
}

int property_get(const char* key, char* value, const char* default_value) {
  int len = 0, ret;

  if (strlen(key) >= PROPERTY_KEY_MAX) {
    printf("invalid key length %d\n", strlen(key));
    return -1;
  }

  ret = fetch_property(key, value);

  if (ret == 1) {
    len = strlen(value);
  } else if (default_value != NULL) {
    snprintf(value, PROPERTY_VALUE_MAX, "%s", default_value);
    len = strlen(value);
  }

  return len;
}

int property_get_int32(const char* key, int default_value) {
  char temp_value[PROPERTY_VALUE_MAX];
  int len;

  len = property_get(key, temp_value, NULL);

  if (len == 0) {
    return default_value;
  } else {
    return atoi(temp_value);
  }
}

int property_get_int32_remote(const char* key, int default_value) {
  return property_get_int32(key, default_value);
}

int property_set(const char* key, char* value) {
  if (key == NULL || value == NULL) {
    printf("property_set invalid arguments!\n");
    return -1;
  }

  update_property(key, value);

  return 0;
}
