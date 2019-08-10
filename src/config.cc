#include "config.h"

#include <sasl/sasl.h>
#include <stdio.h>

#include <algorithm>
#include <fstream>
#include <sstream>

namespace sasl_xoauth2 {

namespace {

constexpr char kConfigFilePath[] = "/etc/sasl/xoauth2.conf";
Config *s_config = nullptr;

template <typename T>
int Transform(std::string in, T *out) {
  fprintf(stderr, "Unknown configuration value type.\n");
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
  fprintf(stderr,
          "Invalid value '%s'. Need either 'yes'/'true' or "
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
    fprintf(stderr, "Missing required configuration value: %s\n", name.c_str());
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
    Json::Value root;
    f >> root;
    s_config = new Config();
    return s_config->Init(root);

  } catch (const std::exception &e) {
    fprintf(stderr, "Exception during config init: %s\n", e.what());
    return SASL_FAIL;
  }
}

int Config::InitForTesting(const Json::Value &root) {
  if (s_config) {
    fprintf(stderr, "Config already initialized!\n");
    exit(1);
  }

  s_config = new Config();
  return s_config->Init(root);
}

Config *Config::Get() {
  if (!s_config) {
    fprintf(stderr, "Attempt to fetch configuration before calling Init()!\n");
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
    fprintf(stderr, "Exception during config init: %s\n", e.what());
    return SASL_FAIL;
  }
}

}  // namespace sasl_xoauth2
