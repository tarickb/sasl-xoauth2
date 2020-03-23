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

#include "http.h"

#include <curl/curl.h>
#include <sasl/sasl.h>
#include <string.h>

#include <vector>

namespace sasl_xoauth2 {

namespace {

constexpr char kUserAgent[] = "sasl xoauth2 token refresher";

class RequestContext {
 public:
  static size_t Read(char *data, size_t size, size_t items, void *context) {
    size *= items;

    auto *request = static_cast<RequestContext *>(context);
    size_t remaining = std::min(request->to_server_remaining_, size);
    memcpy(data, request->to_server_next_, remaining);
    request->to_server_next_ += remaining;
    request->to_server_remaining_ -= remaining;

    return remaining;
  }

  static size_t Write(char *data, size_t size, size_t items, void *context) {
    size *= items;

    auto *request = static_cast<RequestContext *>(context);
    size_t old_size = request->from_server_.size();
    request->from_server_.resize(old_size + size);
    memcpy(&request->from_server_[old_size], data, size);

    return size;
  }

  static int Seek(off_t offset, int origin, void *context) {
    auto *request = static_cast<RequestContext *>(context);
    if (origin != SEEK_SET || offset != 0) return CURL_SEEKFUNC_FAIL;
    request->Rewind();
    return CURL_SEEKFUNC_OK;
  }

  RequestContext(const std::string &data) : to_server_(data) { Rewind(); }

  size_t to_server_size() const { return to_server_.size(); }

  std::string from_server() const {
    return std::string(from_server_.begin(), from_server_.end());
  }

 private:
  void Rewind() {
    to_server_next_ = to_server_.c_str();
    to_server_remaining_ = to_server_.size();
  }

  std::string to_server_;
  const char *to_server_next_ = nullptr;
  size_t to_server_remaining_ = 0;

  std::vector<char> from_server_;
};

HttpIntercept s_intercept = {};

}  // namespace

void SetHttpInterceptForTesting(HttpIntercept intercept) {
  s_intercept = intercept;
}

int HttpPost(const std::string &url, const std::string &data,
             long *response_code, std::string *response, std::string *error) {
  if (s_intercept)
    return s_intercept(url, data, response_code, response, error);

  *response_code = 0;
  response->clear();

  CURL *curl = curl_easy_init();
  if (!curl) {
    *error = "Unable to create CURL handle.";
    return SASL_BADPROT;
  }

  RequestContext context(data);

  char transport_error[CURL_ERROR_SIZE] = {'\0'};

  // Behavior.
  curl_easy_setopt(curl, CURLOPT_VERBOSE, false);
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, true);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, true);

  // Errors.
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, transport_error);

  // Network.
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

  // HTTP.
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, true);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, kUserAgent);
  curl_easy_setopt(curl, CURLOPT_POST, true);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                   static_cast<curl_off_t>(context.to_server_size()));

  // Callbacks.
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &RequestContext::Write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
  curl_easy_setopt(curl, CURLOPT_READFUNCTION, &RequestContext::Read);
  curl_easy_setopt(curl, CURLOPT_READDATA, &context);
  curl_easy_setopt(curl, CURLOPT_SEEKFUNCTION, &RequestContext::Seek);
  curl_easy_setopt(curl, CURLOPT_SEEKDATA, &context);

  CURLcode err = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (err != CURLE_OK) {
    *error = transport_error;
    if (error->empty()) {
      *error = curl_easy_strerror(err);
      *error += " (no further error information)";
    }
    return SASL_BADPROT;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, response_code);
  *response = context.from_server();
  return SASL_OK;
}

}  // namespace sasl_xoauth2
