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

#include "token_store.h"

#include <errno.h>
#include <inttypes.h>
#include <json/json.h>
#include <sasl/sasl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <sstream>

#include "config.h"
#include "http.h"
#include "log.h"

namespace sasl_xoauth2 {

namespace {

constexpr int kMaxRefreshAttempts = 2;
constexpr int kExpiryMarginSec = 10;

std::string GetTempSuffix() {
  timeval t = {};
  gettimeofday(&t, nullptr);
  const uint64_t time_ms = t.tv_sec * 1000 + t.tv_usec / 1000;

  char buf[128];
  snprintf(buf, sizeof(buf), "%d.%" PRIu64, getpid(), time_ms);

  return std::string(buf);
}

void ReadOverride(const Json::Value &root, const std::string &key,
                  std::string *output) {
  if (root.isMember(key)) {
    *output = root[key].asString();
  }
}

void WriteOverride(const std::string &key, const std::string &value,
                   Json::Value *output) {
  if (!value.empty()) {
    (*output)[key] = value;
  }
}

}  // namespace

/* static */ std::unique_ptr<TokenStore> TokenStore::Create(
    Log *log, const std::string &path, bool enable_updates) {
  std::unique_ptr<TokenStore> store(new TokenStore(log, path, enable_updates));
  if (store->Read() != SASL_OK) return {};
  return store;
}

int TokenStore::GetAccessToken(std::string *token) {
  if ((time(nullptr) + kExpiryMarginSec) >= expiry_) {
    log_->Write("TokenStore::GetAccessToken: token expired. refreshing.");
    int err = Refresh();
    if (err != SASL_OK) return err;
  }

  *token = access_;
  return SASL_OK;
}

int TokenStore::Refresh() {
  if (refresh_attempts_ > kMaxRefreshAttempts) {
    log_->Write("TokenStore::Refresh: exceeded maximum attempts");
    return SASL_BADPROT;
  }
  refresh_attempts_++;
  log_->Write("TokenStore::Refresh: attempt %d", refresh_attempts_);

  const std::string client_id =
      (override_client_id_.empty() ? Config::Get()->client_id()
                                   : override_client_id_);
  const std::string client_secret =
      (override_client_secret_.empty() ? Config::Get()->client_secret()
                                       : override_client_secret_);
  const std::string token_endpoint =
      (override_token_endpoint_.empty() ? Config::Get()->token_endpoint()
                                        : override_token_endpoint_);

  const std::string proxy =
      (override_proxy_.empty() ? Config::Get()->proxy() : override_proxy_);

  const std::string ca_bundle_file =
      (override_ca_bundle_file_.empty() ? Config::Get()->ca_bundle_file()
                                        : override_ca_bundle_file_);

  const std::string ca_certs_dir =
      (override_ca_certs_dir_.empty() ? Config::Get()->ca_certs_dir()
                                      : override_ca_certs_dir_);

  const std::string request =
      std::string("client_id=") + client_id +
      "&client_secret=" + client_secret +
      "&grant_type=refresh_token&refresh_token=" + refresh_;
  std::string response;
  long response_code = 0;
  log_->Write("TokenStore::Refresh: token_endpoint: %s",
              token_endpoint.c_str());
  log_->Write("TokenStore::Refresh: request: %s", request.c_str());

  std::string http_error;
  int err = HttpPost(token_endpoint, request, proxy, &response_code, &response,
                     &http_error, ca_bundle_file, ca_certs_dir);
  if (err != SASL_OK) {
    log_->Write("TokenStore::Refresh: http error: %s", http_error.c_str());
    return err;
  }

  log_->Write("TokenStore::Refresh: code=%d, response=%s", response_code,
              response.c_str());

  if (response_code != 200) {
    log_->Write("TokenStore::Refresh: request failed");
    return SASL_BADPROT;
  }

  try {
    std::stringstream ss(response);
    Json::Value root;
    ss >> root;
    if (!root.isMember("access_token") || !root.isMember("expires_in")) {
      log_->Write("TokenStore::Refresh: response doesn't contain access_token");
      return SASL_BADPROT;
    }
    access_ = root["access_token"].asString();
    int expiry_sec = stoi(root["expires_in"].asString());
    if (expiry_sec <= 0) {
      log_->Write("TokenStore::Refresh: invalid expiry");
      return SASL_BADPROT;
    }
    if (root.isMember("refresh_token")) {
      const std::string refresh_token = root["refresh_token"].asString();
      if (refresh_token != refresh_) {
        log_->Write(
            "TokenStore::Refresh: response includes updated refresh token");
        refresh_ = refresh_token;
      }
    }
    expiry_ = time(nullptr) + expiry_sec;
  } catch (const std::exception &e) {
    log_->Write("TokenStore::Refresh: exception=%s", e.what());
    return SASL_FAIL;
  }

  return Write();
}

TokenStore::TokenStore(Log *log, const std::string &path, bool enable_updates)
    : log_(log), path_(path), enable_updates_(enable_updates) {}

int TokenStore::Read() {
  refresh_.clear();
  access_.clear();
  expiry_ = 0;

  try {
    log_->Write("TokenStore::Read: file=%s", path_.c_str());

    std::ifstream file(path_);
    if (!file.good()) {
      log_->Write("TokenStore::Read: failed to open file %s: %s", path_.c_str(),
                  strerror(errno));
      return SASL_FAIL;
    }

    Json::Value root;
    file >> root;
    if (!root.isMember("refresh_token")) {
      log_->Write("TokenStore::Read: missing refresh_token");
      return SASL_FAIL;
    }

    ReadOverride(root, "client_id", &override_client_id_);
    ReadOverride(root, "client_secret", &override_client_secret_);
    ReadOverride(root, "token_endpoint", &override_token_endpoint_);
    ReadOverride(root, "proxy", &override_proxy_);
    ReadOverride(root, "ca_bundle_file", &override_ca_bundle_file_);
    ReadOverride(root, "ca_certs_dir", &override_ca_certs_dir_);

    refresh_ = root["refresh_token"].asString();
    if (root.isMember("access_token"))
      access_ = root["access_token"].asString();
    if (root.isMember("expiry")) expiry_ = stoi(root["expiry"].asString());

    log_->Write("TokenStore::Read: refresh=%s, access=%s", refresh_.c_str(),
                access_.c_str());
    return SASL_OK;

  } catch (const std::exception &e) {
    log_->Write("TokenStore::Read: exception=%s", e.what());
    return SASL_FAIL;
  }
}

int TokenStore::Write() {
  const std::string new_path = path_ + "." + GetTempSuffix();

  if (!enable_updates_) {
    log_->Write("TokenStore::Write: skipping write to %s", new_path.c_str());
    return SASL_OK;
  }

  try {
    Json::Value root;
    root["refresh_token"] = refresh_;
    root["access_token"] = access_;
    root["expiry"] = std::to_string(expiry_);

    WriteOverride("client_id", override_client_id_, &root);
    WriteOverride("client_secret", override_client_secret_, &root);
    WriteOverride("token_endpoint", override_token_endpoint_, &root);
    WriteOverride("proxy", override_proxy_, &root);
    WriteOverride("ca_bundle_file", override_ca_bundle_file_, &root);
    WriteOverride("ca_certs_dir", override_ca_certs_dir_, &root);

    std::ofstream file(new_path);
    if (!file.good()) {
      log_->Write("TokenStore::Write: failed to open file %s for writing: %s",
                  new_path.c_str(), strerror(errno));
      return SASL_FAIL;
    }
    file << root;

  } catch (const std::exception &e) {
    log_->Write("TokenStore::Write: exception=%s", e.what());
    return SASL_FAIL;
  }

  if (rename(new_path.c_str(), path_.c_str()) != 0) {
    log_->Write("TokenStore::Write: rename failed with %s", strerror(errno));
    return SASL_FAIL;
  }

  return 0;
}

}  // namespace sasl_xoauth2
