import http
import http.server
import json
import optparse
import sys
import urllib.error
import urllib.parse
import urllib.request

OAUTH2_AUTH_URL = 'https://accounts.google.com/o/oauth2/auth'
OAUTH2_TOKEN_URL = 'https://accounts.google.com/o/oauth2/token'
OAUTH2_RESULT_PATH = '/oauth2_result'

PARSER = optparse.OptionParser(usage='Usage: %prog [options] output-token-file')
PARSER.add_option('--client_id', dest='client_id', default='')
PARSER.add_option('--client_secret', dest='client_secret', default='')
PARSER.add_option('--scope', dest='scope', default='')

OPTIONS, CMDLINE_ARGS = PARSER.parse_args()

def UrlSafeEscape(str):
  return urllib.parse.quote(str, safe='~-._')

def RedirectUri(local_port):
  return 'http://127.0.0.1:%d%s' % (local_port, OAUTH2_RESULT_PATH)

def GetAuthUrl(local_port):
  client_id = UrlSafeEscape(OPTIONS.client_id)
  scope = UrlSafeEscape(OPTIONS.scope)
  redirect_uri = UrlSafeEscape(RedirectUri(local_port))

  return '%s?client_id=%s&scope=%s&response_type=%s&redirect_uri=%s' % (OAUTH2_AUTH_URL, client_id, scope, 'code', redirect_uri)

def GetTokenFromCode(authorization_code, local_port):
  params = {}
  params['client_id'] = OPTIONS.client_id
  params['client_secret'] = OPTIONS.client_secret
  params['code'] = authorization_code
  params['redirect_uri'] = RedirectUri(local_port)
  params['grant_type'] = 'authorization_code'

  data = urllib.parse.urlencode(params)
  response = urllib.request.urlopen(OAUTH2_TOKEN_URL, data.encode('ascii')).read()
  return json.loads(response)

class RequestHandler(http.server.BaseHTTPRequestHandler):
  def log_request(code, size):
    # Silence request logging.
    return

  def do_GET(self):
    code = self.ExtractCodeFromResponse()

    response_code = 400
    response_text = '<html><head><title>Error</title></head><body><h1>Invalid request.</h1></body></html>'

    if code:
      response_code = 200
      response_text = '<html><head><title>Done</title></head><body><h1>You may close this window now.</h1></body></html>'

    self.send_response(response_code)
    self.send_header('Content-type', 'text/html')
    self.end_headers()
    self.wfile.write(response_text.encode('utf8'))

    if code:
      token = GetTokenFromCode(code,
          self.server.server_address[1])
      with open(CMDLINE_ARGS[0], 'w') as f:
        json.dump(token, f)
      print('Wrote token to %s.' % CMDLINE_ARGS[0])
      sys.exit(0)

  def ExtractCodeFromResponse(self):
    parse = urllib.parse.urlparse(self.path)
    if parse.path != OAUTH2_RESULT_PATH:
      return None
    qs = urllib.parse.parse_qs(parse.query)
    if 'code' not in qs:
      return None
    if len(qs['code']) != 1:
      return None
    return qs['code'][0]

def main():
  if len(CMDLINE_ARGS) != 1:
    PARSER.print_usage()
    sys.exit(1)

  if len(OPTIONS.client_id) == 0:
    print('Please specify a client ID.')
    sys.exit(1)

  if len(OPTIONS.client_secret) == 0:
    print('Please specify a client secret.')
    sys.exit(1)

  if len(OPTIONS.scope) == 0:
    print('Please specify a scope.')
    sys.exit(1)

  server = http.server.HTTPServer(('', 0), RequestHandler)
  _, port = server.server_address

  url = GetAuthUrl(port)
  print('Please open this URL in a browser ON THIS HOST:\n\n%s\n' % url)

  server.serve_forever()

if __name__ == '__main__':
  main()
