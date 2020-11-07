# Copyright 2020 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import httplib2
import json
import optparse
import sys
import urllib.parse

REDIRECT_URI = "https://login.microsoftonline.com/common/oauth2/nativeclient"
SCOPE = "openid offline_access https://outlook.office.com/SMTP.Send"

PARSER = optparse.OptionParser(usage='Usage: %prog [options] output-token-file')
PARSER.add_option('--tenant', dest='tenant', default='consumers')
PARSER.add_option('--client_id', dest='client_id', default='')

OPTIONS, CMDLINE_ARGS = PARSER.parse_args()

def get_authorization_code():
  url = "https://login.microsoftonline.com/%s/oauth2/v2.0/authorize" % OPTIONS.tenant
  url += "?client_id=%s" % OPTIONS.client_id
  url += "&response_type=code"
  url += "&redirect_uri=%s" % urllib.parse.quote(REDIRECT_URI)
  url += "&response_mode=query"
  url += "&scope=%s" % urllib.parse.quote(SCOPE)

  print("Please visit the following link in a web browser, then paste the resulting URL:\n\n%s\n" % url)

  resulting_url = input("Resulting URL: ")
  if REDIRECT_URI not in resulting_url:
    raise Exception("Resulting URL does not contain expected prefix: %s" % REDIRECT_URI)
  resulting_url = urllib.parse.urlparse(resulting_url)
  code = urllib.parse.parse_qs(resulting_url.query)
  if "code" not in code:
    raise Exception("Missing code in result: %s" % resulting_url.query)
  return code["code"][0]

def get_initial_tokens(code):
  url = "https://login.microsoftonline.com/%s/oauth2/v2.0/token" % OPTIONS.tenant
  token_request = "client_id=%s" % OPTIONS.client_id
  token_request += "&scope=%s" % urllib.parse.quote(SCOPE)
  token_request += "&code=%s" % code
  token_request += "&redirect_uri=%s" % urllib.parse.quote(REDIRECT_URI)
  token_request += "&grant_type=authorization_code"

  http = httplib2.Http()
  resp, content = http.request(
      uri=url,
      method="POST",
      headers={ "Content-Type": "application/x-www-form-urlencoded" },
      body=token_request)
  content = content.decode("utf-8")
  if resp.status != 200:
    raise Exception("Request failed: %s" % content)
  try:
    content = json.loads(content)
    return {
        'access_token': content["access_token"],
        'refresh_token': content["refresh_token"],
        'expiry': 0 }
  except:
    raise Exception("Tokens not found in response: %s" % content)

def main():
  if len(CMDLINE_ARGS) != 1:
    PARSER.print_usage()
    sys.exit(1)

  if len(OPTIONS.client_id) == 0:
    print('Please specify a client ID.')
    sys.exit(1)

  output_path = CMDLINE_ARGS[0]
  code = get_authorization_code()
  tokens = get_initial_tokens(code)

  with open(output_path, 'w') as f:
    json.dump(tokens, f)

  print('Tokens written to %s.' % output_path)

main()
