#ifndef SASL_XOAUTH2_HTTP_H
#define SASL_XOAUTH2_HTTP_H

#include <functional>
#include <string>

namespace sasl_xoauth2 {

using HttpIntercept = std::function<int(
    const std::string &url, const std::string &data, int *response_code,
    std::string *response, std::string *error)>;

void SetHttpInterceptForTesting(HttpIntercept intercept);

int HttpPost(const std::string &url, const std::string &data,
             int *response_code, std::string *response, std::string *error);

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_HTTP_H
