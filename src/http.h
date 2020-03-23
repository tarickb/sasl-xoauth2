/*
 * Copyright 2020 Google LLC
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

#ifndef SASL_XOAUTH2_HTTP_H
#define SASL_XOAUTH2_HTTP_H

#include <functional>
#include <string>

namespace sasl_xoauth2 {

using HttpIntercept = std::function<int(
    const std::string &url, const std::string &data, long *response_code,
    std::string *response, std::string *error)>;

void SetHttpInterceptForTesting(HttpIntercept intercept);

int HttpPost(const std::string &url, const std::string &data,
             long *response_code, std::string *response, std::string *error);

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_HTTP_H
