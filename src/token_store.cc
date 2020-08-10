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
#include <sasl/sasl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <json/json.h>
#include <fstream>
#include <sstream>

#include "config.h"
#include "http.h"
#include "log.h"

namespace sasl_xoauth2 {

namespace {

constexpr int kMaxRefreshAttempts = 2;
constexpr int kExpiryMarginSec = 10;

constexpr char kTokenEndpoint[] = "https://accounts.google.com/o/oauth2/token";

std::string GetTempSuffix() {
  timeval t = {};
  gettimeofday(&t, nullptr);
  const uint64_t time_ms = t.tv_sec * 1000 + t.tv_usec / 1000;

  char buf[128];
  snprintf(buf, sizeof(buf), "%d.%" PRIu64, getpid(), time_ms);

  return std::string(buf);
}

}  // namespace

/* static */ std::unique_ptr<TokenStore> TokenStore::Create(
    Log *log, const std::string &path) {
  std::unique_ptr<TokenStore> store(new TokenStore(log, path));
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

  const std::string request =
      std::string("client_id=") + Config::Get()->client_id() +
      "&client_secret=" + Config::Get()->client_secret() +
      "&grant_type=refresh_token&refresh_token=" + refresh_;
  std::string response;
  long response_code = 0;
  log_->Write("TokenStore::Refresh: request: %s", request.c_str());

  std::string http_error;
  int err =
      HttpPost(kTokenEndpoint, request, &response_code, &response, &http_error);
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
    expiry_ = time(nullptr) + expiry_sec;
  } catch (const std::exception &e) {
    log_->Write("TokenStore::Refresh: exception=%s", e.what());
    return SASL_FAIL;
  }

  return Write();
}

TokenStore::TokenStore(Log *log, const std::string &path)
    : log_(log), path_(path) {}

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

  try {
    Json::Value root;
    root["refresh_token"] = refresh_;
    root["access_token"] = access_;
    root["expiry"] = std::to_string(expiry_);

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
