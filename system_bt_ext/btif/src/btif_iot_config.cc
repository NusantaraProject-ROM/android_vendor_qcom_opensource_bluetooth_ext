/******************************************************************************
 *  Copyright (C) 2017, The Linux Foundation. All rights reserved.
 *
 *  Not a Contribution
 *****************************************************************************/
/******************************************************************************
 *
 *  Copyright (C) 2014 Google, Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/
#ifdef BT_IOT_LOGGING_ENABLED

#define LOG_TAG "btif_iot_config"
//#undef LOG_NDEBUG
//#define LOG_NDEBUG 0
#include "btif_iot_config.h"

#include <base/logging.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <mutex>

#include "bt_types.h"
#include "btcore/include/module.h"
#include "btif_api.h"
#include "btif_common.h"
#include "btif_util.h"
#include "osi/include/alarm.h"
#include "osi/include/allocator.h"
#include "osi/include/compat.h"
#include "osi/include/config.h"
#include "osi/include/log.h"
#include "osi/include/osi.h"

#include "osi/include/properties.h"

#define BT_IOT_CONFIG_SOURCE_TAG_NUM 1010003

#define INFO_SECTION "Info"
#define FILE_CREATED_TIMESTAMP "TimeCreated"
#define FILE_MODIFIED_TIMESTAMP "TimeModified"
#define TIME_STRING_LENGTH sizeof("YYYY-MM-DD HH:MM:SS")
static const char* TIME_STRING_FORMAT = "%Y-%m-%d %H:%M:%S";

#if defined(OS_GENERIC)
static const char* IOT_CONFIG_FILE_PATH = "bt_remote_dev_info.conf";
static const char* IOT_CONFIG_BACKUP_PATH = "bt_remote_dev_info.bak";
#else  // !defined(OS_GENERIC)
static const char* IOT_CONFIG_FILE_PATH = "/data/misc/bluedroid/bt_remote_dev_info.conf";
static const char* IOT_CONFIG_BACKUP_PATH = "/data/misc/bluedroid/bt_remote_dev_info.bak";
#endif  // defined(OS_GENERIC)
static const period_ms_t CONFIG_SETTLE_PERIOD_MS = 6000;

#define IOT_CONFIG_FLUSH_EVT            0
#define IOT_CONFIG_SAVE_TIMER_FIRED_EVT 1

static void timer_iot_config_save_cb(void* data);
static void btif_iot_config_write(uint16_t event, char* p_param);
static config_t* btif_iot_config_open(const char* filename);
static int btif_iot_config_get_device_num(config_t* config);


static enum ConfigSource {
  NOT_LOADED,
  ORIGINAL,
  BACKUP,
  NEW_FILE,
  RESET
} btif_iot_config_source = NOT_LOADED;

static int btif_iot_config_devices_loaded = -1;
static char btif_iot_config_time_created[TIME_STRING_LENGTH];

static std::mutex config_lock;  // protects operations on |config|.
static config_t* config;
static alarm_t* config_timer;
static bool iot_logging_enabled = true;

#define CHECK_LOGGING_ENABLED(return_value) do { if (!iot_logging_enabled) return (return_value); } while(0)

// Module lifecycle functions
static future_t* init(void) {
  char enabled[PROPERTY_VALUE_MAX] = {0};

  osi_property_get("persist.vendor.service.bdroid.iot.enablelogging", enabled, "false");
  iot_logging_enabled = strncmp(enabled, "true", 4) == 0;

  if (!iot_logging_enabled) {
    delete_iot_config_files();
    return future_new_immediate(FUTURE_SUCCESS);
  }

  LOG_INFO(LOG_TAG, "%s", __func__);
  std::unique_lock<std::mutex> lock(config_lock);
  config = btif_iot_config_open(IOT_CONFIG_FILE_PATH);
  btif_iot_config_source = ORIGINAL;
  if (!config) {
    LOG_WARN(LOG_TAG, "%s unable to load config file: %s; using backup.",
          __func__, IOT_CONFIG_FILE_PATH);
    config = btif_iot_config_open(IOT_CONFIG_BACKUP_PATH);
    btif_iot_config_source = BACKUP;
  }

  if (!config) {
    LOG_ERROR(LOG_TAG, "%s unable to load bak file; creating empty config.", __func__);
    config = config_new_empty();
    btif_iot_config_source = NEW_FILE;
  }

  if (!config) {
    LOG_ERROR(LOG_TAG, "%s unable to allocate a config object.", __func__);
    goto error;
  }

  btif_iot_config_devices_loaded = btif_iot_config_get_device_num(config);

  // Read or set config file creation timestamp
  const char* time_str;
  time_str = config_get_string(config, INFO_SECTION, FILE_CREATED_TIMESTAMP, NULL);
  if (time_str != NULL) {
    strlcpy(btif_iot_config_time_created, time_str, TIME_STRING_LENGTH);
  } else {
    time_t current_time = time(NULL);
    struct tm* time_created = localtime(&current_time);
    if (time_created) {
      strftime(btif_iot_config_time_created, TIME_STRING_LENGTH, TIME_STRING_FORMAT, time_created);
      config_set_string(config, INFO_SECTION, FILE_CREATED_TIMESTAMP, btif_iot_config_time_created);
    }
  }

  // TODO(sharvil): use a non-wake alarm for this once we have
  // API support for it. There's no need to wake the system to
  // write back to disk.
  config_timer = alarm_new("btif.iot.config");
  if (!config_timer) {
    LOG_ERROR(LOG_TAG, "%s unable to create alarm.", __func__);
    goto error;
  }

  LOG_EVENT_INT(BT_IOT_CONFIG_SOURCE_TAG_NUM, btif_iot_config_source);

  return future_new_immediate(FUTURE_SUCCESS);

error:
  alarm_free(config_timer);
  config_free(config);
  config_timer = NULL;
  config = NULL;
  btif_iot_config_source = NOT_LOADED;
  return future_new_immediate(FUTURE_FAIL);
}

static config_t* btif_iot_config_open(const char* filename) {
  config_t* config = config_new(filename);
  if (!config)
    return NULL;

  return config;
}

static future_t* start_up(void) {
  CHECK_LOGGING_ENABLED(future_new_immediate(FUTURE_SUCCESS));

  LOG_INFO(LOG_TAG, "%s", __func__);
  btif_iot_config_int_add_one(IOT_CONF_KEY_SECTION_ADAPTER, IOT_CONF_KEY_BT_ONOFF_COUNT);
  return future_new_immediate(FUTURE_SUCCESS);
}

static future_t* shut_down(void) {
  CHECK_LOGGING_ENABLED(future_new_immediate(FUTURE_SUCCESS));

  LOG_INFO(LOG_TAG, "%s", __func__);
  btif_iot_config_flush();
  return future_new_immediate(FUTURE_SUCCESS);
}

static future_t* clean_up(void) {
  CHECK_LOGGING_ENABLED(future_new_immediate(FUTURE_SUCCESS));

  LOG_INFO(LOG_TAG, "%s", __func__);
  btif_iot_config_flush();

  alarm_free(config_timer);
  config_timer = NULL;

  std::unique_lock<std::mutex> lock(config_lock);
  config_free(config);
  config = NULL;
  return future_new_immediate(FUTURE_SUCCESS);
}

EXPORT_SYMBOL module_t btif_iot_config_module = {
  .name = BTIF_IOT_CONFIG_MODULE,
  .init = init,
  .start_up = start_up,
  .shut_down = shut_down,
  .clean_up = clean_up
};

bool btif_iot_config_has_section(const char* section) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);

  std::unique_lock<std::mutex> lock(config_lock);
  return config_has_section(config, section);
}

bool btif_iot_config_exist(const char* section, const char* key) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);

  std::unique_lock<std::mutex> lock(config_lock);
  return config_has_key(config, section, key);
}

bool btif_iot_config_get_int(const char* section, const char* key, int* value) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);
  CHECK(value != NULL);

  std::unique_lock<std::mutex> lock(config_lock);
  bool ret = config_has_key(config, section, key);
  if (ret)
    *value = config_get_int(config, section, key, *value);

  return ret;
}

bool btif_iot_config_addr_get_int(const RawAddress& peer_addr, const char* key, int* value) {
  CHECK_LOGGING_ENABLED(false);

  std::string addrstr = peer_addr.ToString();
  const char* bdstr = addrstr.c_str();
  return btif_iot_config_get_int(bdstr, key, value);
}

bool btif_iot_config_set_int(const char* section, const char* key, int value) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);

  LOG_VERBOSE(LOG_TAG, "%s: sec=%s, key=%s, val=%d", __func__, section, key, value);
  bool changed;
  std::unique_lock<std::mutex> lock(config_lock);
  changed = config_set_int(config, section, key, value);
  if (changed)
    btif_iot_config_save();

  return true;
}

bool btif_iot_config_addr_set_int(const RawAddress& peer_addr, const char* key, int value) {
  CHECK_LOGGING_ENABLED(false);

  std::string addrstr = peer_addr.ToString();
  const char* bdstr = addrstr.c_str();
  return btif_iot_config_set_int(bdstr, key, value);
}

bool btif_iot_config_int_add_one(const char* section, const char* key) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);

  LOG_VERBOSE(LOG_TAG, "%s: sec=%s, key=%s", __func__, section, key);
  int result = 0;
  std::unique_lock<std::mutex> lock(config_lock);
  result = config_get_int(config, section, key, result);
  result += 1;
  config_set_int(config, section, key, result);
  btif_iot_config_save();

  return true;
}

bool btif_iot_config_addr_int_add_one(const RawAddress& peer_addr, const char* key) {
  CHECK_LOGGING_ENABLED(false);

  std::string addrstr = peer_addr.ToString();
  const char* bdstr = addrstr.c_str();
  return btif_iot_config_int_add_one(bdstr, key);
}

bool btif_iot_config_get_hex(const char* section, const char* key, int* value) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);
  CHECK(value != NULL);

  int sscanf_ret, result = 0;
  std::unique_lock<std::mutex> lock(config_lock);
  const char* stored_value = config_get_string(config, section, key, NULL);
  if (!stored_value)
    return false;

  sscanf_ret = sscanf(stored_value, "%x", &result);
  if (sscanf_ret != 1)
    return false;

  *value = result;
  return true;
}

bool btif_iot_config_addr_get_hex(const RawAddress& peer_addr, const char* key, int* value) {
  CHECK_LOGGING_ENABLED(false);

  std::string addrstr = peer_addr.ToString();
  const char* bdstr = addrstr.c_str();
  return btif_iot_config_get_hex(bdstr, key, value);
}

bool btif_iot_config_set_hex(const char* section, const char* key, int value, int byte_num) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);

  LOG_VERBOSE(LOG_TAG, "%s: sec=%s, key=%s, val=0x%x", __func__, section, key, value);
  bool changed;
  char value_str[32] = { 0 };
  if (byte_num == 1)
    sprintf(value_str, "%02x", value);
  else if (byte_num == 2)
    sprintf(value_str, "%04x", value);
  else if (byte_num == 3)
    sprintf(value_str, "%06x", value);
  else if (byte_num == 4)
    sprintf(value_str, "%08x", value);

  std::unique_lock<std::mutex> lock(config_lock);
  changed = config_set_string(config, section, key, value_str);
  if (changed)
    btif_iot_config_save();

  return true;
}

bool btif_iot_config_addr_set_hex(const RawAddress& peer_addr, const char* key, int value, int byte_num) {
  CHECK_LOGGING_ENABLED(false);

  std::string addrstr = peer_addr.ToString();
  const char* bdstr = addrstr.c_str();
  return btif_iot_config_set_hex(bdstr, key, value, byte_num);
}

bool btif_iot_config_addr_set_hex_if_greater(const RawAddress& peer_addr,
    const char* key, int value, int byte_num) {
  CHECK_LOGGING_ENABLED(false);

  int stored_value = 0;
  bool ret = btif_iot_config_addr_get_hex(peer_addr, key, &stored_value);
  if (ret && stored_value >= value)
    return true;

  return btif_iot_config_addr_set_hex(peer_addr, key, value, byte_num);
}

bool btif_iot_config_get_str(const char* section, const char* key, char* value, int* size_bytes) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);
  CHECK(value != NULL);
  CHECK(size_bytes != NULL);

  std::unique_lock<std::mutex> lock(config_lock);
  const char* stored_value = config_get_string(config, section, key, NULL);

  if (!stored_value)
    return false;

  strlcpy(value, stored_value, *size_bytes);
  *size_bytes = strlen(value) + 1;

  return true;
}

bool btif_iot_config_set_str(const char* section, const char* key, const char* value) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);
  CHECK(value != NULL);

  LOG_VERBOSE(LOG_TAG, "%s: sec=%s, key=%s, val=%s", __func__, section, key, value);
  bool changed;
  std::unique_lock<std::mutex> lock(config_lock);
  changed = config_set_string(config, section, key, value);
  if (changed)
    btif_iot_config_save();

  return true;
}

bool btif_iot_config_addr_set_str(const RawAddress& peer_addr, const char* key, const char* value) {
  CHECK_LOGGING_ENABLED(false);

  std::string addrstr = peer_addr.ToString();
  const char* bdstr = addrstr.c_str();
  return btif_iot_config_set_str(bdstr, key, value);
}

bool btif_iot_config_get_bin(const char* section, const char* key, uint8_t* value, size_t* length) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);
  CHECK(value != NULL);
  CHECK(length != NULL);

  std::unique_lock<std::mutex> lock(config_lock);
  const char* value_str = config_get_string(config, section, key, NULL);

  if (!value_str)
    return false;

  size_t value_len = strlen(value_str);
  if ((value_len % 2) != 0 || *length < (value_len / 2))
    return false;

  for (size_t i = 0; i < value_len; ++i)
    if (!isxdigit(value_str[i]))
      return false;

  for (*length = 0; *value_str; value_str += 2, *length += 1)
    sscanf(value_str, "%02hhx", &value[*length]);

  return true;
}

size_t btif_iot_config_get_bin_length(const char* section, const char* key) {
  CHECK_LOGGING_ENABLED(0);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);

  std::unique_lock<std::mutex> lock(config_lock);
  const char* value_str = config_get_string(config, section, key, NULL);

  if (!value_str)
    return 0;

  size_t value_len = strlen(value_str);
  return ((value_len % 2) != 0) ? 0 : (value_len / 2);
}

bool btif_iot_config_set_bin(const char* section, const char* key, const uint8_t* value, size_t length) {
  CHECK_LOGGING_ENABLED(false);

  const char* lookup = "0123456789abcdef";
  bool changed;

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);

  LOG_VERBOSE(LOG_TAG, "%s: key = %s", __func__, key);
  if (length > 0)
    CHECK(value != NULL);

  char* str = (char* )osi_calloc(length * 2 + 1);

  for (size_t i = 0; i < length; ++i) {
    str[(i * 2) + 0] = lookup[(value[i] >> 4) & 0x0F];
    str[(i * 2) + 1] = lookup[value[i] & 0x0F];
  }

  std::unique_lock<std::mutex> lock(config_lock);
  changed = config_set_string(config, section, key, str);
  if (changed)
    btif_iot_config_save();

  osi_free(str);
  return true;
}

bool btif_iot_config_addr_set_bin(const RawAddress& peer_addr, const char* key, const uint8_t* value, size_t length) {
  CHECK_LOGGING_ENABLED(false);

  std::string addrstr = peer_addr.ToString();
  const char* bdstr = addrstr.c_str();
  return btif_iot_config_set_bin(bdstr, key, value, length);
}

const btif_iot_config_section_iter_t* btif_iot_config_section_begin(void) {
  CHECK_LOGGING_ENABLED(NULL);

  CHECK(config != NULL);
  return (const btif_iot_config_section_iter_t* )config_section_begin(config);
}

const btif_iot_config_section_iter_t* btif_iot_config_section_end(void) {
  CHECK_LOGGING_ENABLED(NULL);

  CHECK(config != NULL);
  return (const btif_iot_config_section_iter_t* )config_section_end(config);
}

const btif_iot_config_section_iter_t* btif_iot_config_section_next(const btif_iot_config_section_iter_t* section) {
  CHECK_LOGGING_ENABLED(NULL);

  CHECK(config != NULL);
  CHECK(section != NULL);
  return (const btif_iot_config_section_iter_t* )config_section_next((const config_section_node_t* )section);
}

const char* btif_iot_config_section_name(const btif_iot_config_section_iter_t* section) {
  CHECK_LOGGING_ENABLED(NULL);

  CHECK(config != NULL);
  CHECK(section != NULL);
  return config_section_name((const config_section_node_t* )section);
}

bool btif_iot_config_remove(const char* section, const char* key) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(section != NULL);
  CHECK(key != NULL);

  std::unique_lock<std::mutex> lock(config_lock);
  return config_remove_key(config, section, key);
}

void btif_iot_config_save(void) {
  CHECK_LOGGING_ENABLED((void)0);

  CHECK(config != NULL);
  CHECK(config_timer != NULL);

  LOG_VERBOSE(LOG_TAG, "%s", __func__);
  alarm_set(config_timer, CONFIG_SETTLE_PERIOD_MS, timer_iot_config_save_cb, NULL);
}

void btif_iot_config_flush(void) {
  CHECK_LOGGING_ENABLED((void)0);

  CHECK(config != NULL);
  CHECK(config_timer != NULL);

  int event = alarm_is_scheduled(config_timer) ? IOT_CONFIG_SAVE_TIMER_FIRED_EVT : IOT_CONFIG_FLUSH_EVT;
  LOG_VERBOSE(LOG_TAG, "%s: evt=%d", __func__, event);
  alarm_cancel(config_timer);
  btif_iot_config_write(event, NULL);
}

bool btif_iot_config_clear(void) {
  CHECK_LOGGING_ENABLED(false);

  CHECK(config != NULL);
  CHECK(config_timer != NULL);

  LOG_INFO(LOG_TAG, "%s", __func__);
  alarm_cancel(config_timer);

  std::unique_lock<std::mutex> lock(config_lock);
  config_free(config);

  config = config_new_empty();
  if (config == NULL) {
    return false;
  }

  bool ret = config_save(config, IOT_CONFIG_FILE_PATH);
  btif_iot_config_source = RESET;
  return ret;
}

static void timer_iot_config_save_cb(UNUSED_ATTR void* data) {
  // Moving file I/O to btif context instead of timer callback because
  // it usually takes a lot of time to be completed, introducing
  // delays during A2DP playback causing blips or choppiness.
  LOG_VERBOSE(LOG_TAG, "%s", __func__);
  btif_transfer_context(btif_iot_config_write, IOT_CONFIG_SAVE_TIMER_FIRED_EVT, NULL, 0, NULL);
}

static void set_modified_time() {
  time_t current_time = time(NULL);
  struct tm* time_modified = localtime(&current_time);
  char btif_iot_config_time_modified[TIME_STRING_LENGTH];
  if (time_modified) {
    strftime(btif_iot_config_time_modified, TIME_STRING_LENGTH, TIME_STRING_FORMAT, time_modified);
    config_set_string(config, INFO_SECTION, FILE_MODIFIED_TIMESTAMP, btif_iot_config_time_modified);
  }
}

static int compare_key(const char* first, const char* second) {
  bool first_is_profile_key = strncasecmp(first, "Profile", 7) == 0;
  bool second_is_profile_key = strncasecmp(second, "Profile", 7) == 0;
  if (!first_is_profile_key && !second_is_profile_key) {
    return 0;
  } else if (first_is_profile_key && second_is_profile_key) {
    return strcasecmp(first, second);
  } else {
    return first_is_profile_key ? 1 : -1;
  }
}

static void btif_iot_config_write(uint16_t event, UNUSED_ATTR char* p_param) {
  CHECK_LOGGING_ENABLED((void)0);

  CHECK(config != NULL);
  CHECK(config_timer != NULL);

  LOG_INFO(LOG_TAG, "%s: evt=%d", __func__, event);
  std::unique_lock<std::mutex> lock(config_lock);
  if (event == IOT_CONFIG_SAVE_TIMER_FIRED_EVT)
    set_modified_time();
  rename(IOT_CONFIG_FILE_PATH, IOT_CONFIG_BACKUP_PATH);
  config_sections_sort_by_entry_key(config, compare_key);
  config_save(config, IOT_CONFIG_FILE_PATH);
}

static int btif_iot_config_get_device_num(config_t* conf) {
  CHECK_LOGGING_ENABLED(0);

  CHECK(conf != NULL);
  int paired_devices = 0;

  const config_section_node_t* snode = config_section_begin(conf);
  while (snode != config_section_end(conf)) {
    const char* section = config_section_name(snode);
    if (RawAddress::IsValidAddress(section)) {
      paired_devices++;
    }
    snode = config_section_next(snode);
  }
  return paired_devices;
}

void btif_debug_iot_config_dump(int fd) {
  CHECK_LOGGING_ENABLED((void)0);

  dprintf(fd, "\nBluetooth Iot Config:\n");

  dprintf(fd, "  Config Source: ");
  switch(btif_iot_config_source) {
    case NOT_LOADED:
      dprintf(fd, "Not loaded\n");
      break;
    case ORIGINAL:
      dprintf(fd, "Original file\n");
      break;
    case BACKUP:
      dprintf(fd, "Backup file\n");
      break;
    case NEW_FILE:
      dprintf(fd, "New file\n");
      break;
    case RESET:
      dprintf(fd, "Reset file\n");
      break;
  }

  dprintf(fd, "  Devices loaded: %d\n", btif_iot_config_devices_loaded);
  dprintf(fd, "  File created/tagged: %s\n", btif_iot_config_time_created);
}

void delete_iot_config_files(void) {
  std::unique_lock<std::mutex> lock(config_lock);
  remove(IOT_CONFIG_FILE_PATH);
  remove(IOT_CONFIG_BACKUP_PATH);
}

#endif
