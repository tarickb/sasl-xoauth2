// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "config.h"

#include <errno.h>
#include <sasl/sasl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace sasl_xoauth2 {

namespace {

constexpr char kConfigFilePath[] = CONFIG_FILE_FULL_PATH;

bool s_test_mode = false;
Config *s_config = nullptr;

void Log(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  if (s_test_mode)
    vfprintf(stderr, fmt, args);
  else
    vsyslog(LOG_WARNING, fmt, args);

  va_end(args);
}

template <typename T>
int Transform(std::string in, T *out) {
  Log("sasl-xoauth2: Unknown value type.\n");
  return SASL_FAIL;
}

template <>
int Transform(std::string in, bool *out) {
  std::for_each(in.begin(), in.end(), [](char c) { return std::tolower(c); });
  if (in == "yes" || in == "true") {
    *out = true;
    return SASL_OK;
  }
  if (in == "no" || in == "false") {
    *out = false;
    return SASL_OK;
  }
  Log("sasl-xoauth2: Invalid value '%s'. Need either 'yes'/'true' or "
      "'no'/'false'.\n",
      in.c_str());
  return SASL_FAIL;
}

template <>
int Transform(std::string in, std::string *out) {
  *out = in;
  return SASL_OK;
}

template <typename T>
int Fetch(const Json::Value &root, const std::string &name, bool optional,
          T *out) {
  if (!root.isMember(name)) {
    if (optional) return SASL_OK;
    Log("sasl-xoauth2: Missing required value: %s\n", name.c_str());
    return SASL_FAIL;
  }
  return Transform(root[name].asString(), out);
}

}  // namespace

int Config::Init() {
  // Fail silently if we've already been initialized (via InitForTesting, say).
  if (s_config) return SASL_OK;

  try {
    std::ifstream f(kConfigFilePath);
    if (!f.good()) {
      Log("sasl-xoauth2: Unable to open config file %s: %s\n", kConfigFilePath,
          strerror(errno));
      return SASL_FAIL;
    }

    Json::Value root;
    f >> root;
    s_config = new Config();
    return s_config->Init(root);

  } catch (const std::exception &e) {
    Log("sasl-xoauth2: Exception during init: %s\n", e.what());
    return SASL_FAIL;
  }
}

int Config::InitForTesting(const Json::Value &root) {
  if (s_config) {
    Log("sasl-xoauth2: Already initialized!\n");
    exit(1);
  }

  s_test_mode = true;
  s_config = new Config();
  return s_config->Init(root);
}

Config *Config::Get() {
  if (!s_config) {
    Log("sasl-xoauth2: Attempt to fetch before calling Init()!\n");
    exit(1);
  }
  return s_config;
}

int Config::Init(const Json::Value &root) {
  try {
    int err;

    err = Fetch(root, "client_id", false, &client_id_);
    if (err != SASL_OK) return err;

    err = Fetch(root, "client_secret", false, &client_secret_);
    if (err != SASL_OK) return err;

    err = Fetch(root, "log_to_syslog_on_failure", true,
                &log_to_syslog_on_failure_);
    if (err != SASL_OK) return err;

    err = Fetch(root, "log_full_trace_on_failure", true,
                &log_full_trace_on_failure_);
    if (err != SASL_OK) return err;

    return 0;

  } catch (const std::exception &e) {
    Log("sasl-xoauth2: Exception during init: %s\n", e.what());
    return SASL_FAIL;
  }
}

}  // namespace sasl_xoauth2
