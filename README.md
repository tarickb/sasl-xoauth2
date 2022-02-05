# sasl-xoauth2

## Disclaimer

This is not an officially supported Google product.

## Background

sasl-xoauth2 is a SASL plugin that enables client-side use of OAuth 2.0. Among
other things it enables the use of Gmail or Outlook/Office 365 SMTP relays from
Postfix.

## Building from Source

Fetch the sources, then:

```
$ mkdir build && cd build && cmake ..
# To install with a system-packaged postfix, under /usr, use:
# cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc
$ make
$ sudo make install
```

## Pre-Built Packages for Ubuntu

Add the [sasl-xoauth2 PPA](https://launchpad.net/~sasl-xoauth2/+archive/ubuntu/stable):

```
$ sudo add-apt-repository ppa:sasl-xoauth2/stable
$ sudo apt-get update
```

Install the plugin:

```
$ sudo apt-get install sasl-xoauth2
```

## Configuration

### Configure Mail Agent

This plugin has only been tested with Postfix. First, configure Postfix to use
SASL, and specifically the XOAUTH2 method. In `/etc/postfix/main.cf`:

```
smtp_sasl_auth_enable = yes
smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd
smtp_sasl_security_options =
smtp_sasl_mechanism_filter = xoauth2
```

Alternatively, you could specify multiple mechanisms, i.e. 
```
smtp_sasl_mechanism_filter = xoauth2,login
```

The above used to cause problems, because both "xoauth2" and the "login"
plug-in (as is used for many older, not-yet-OAuth2 providers) had the
same "SSF" setting of "0". This made SASL's automatic detection of which
plug-in to use non-deterministic. Now, with the higher SSF of "60" for
"xoauth2", providers offering OAUTH2 will be handled via the xoauth2 plug-in.

You can check the effective value by calling `pluginviewer -c` (on Debian/Ubuntu it’s called `saslpluginviewer`); look for
the "SSF" value:
```
Plugin "sasl-xoauth2" [loaded],         API version: 4
        SASL mechanism: XOAUTH2, best SSF: 60
        security flags: NO_ANONYMOUS|PASS_CREDENTIALS
        features: WANT_CLIENT_FIRST|PROXY_AUTHENTICATION
Plugin "login" [loaded],        API version: 4
        SASL mechanism: LOGIN, best SSF: 0
        security flags: NO_ANONYMOUS|PASS_CREDENTIALS
        features: SERVER_FIRST
```
See https://www.cyrusimap.org/sasl/sasl/authentication_mechanisms.html#authentication-mechanisms for details on the fields.


While you're at it, enable TLS:

```
smtp_tls_security_level = encrypt
smtp_tls_CAfile = /etc/ssl/certs/ca-certificates.crt
smtp_tls_session_cache_database = btree:${data_directory}/smtp_scache
```

And then set the outbound relay to Gmail's SMTP server:

```
relayhost = [smtp.gmail.com]:587
```

(For Outlook, use `[smtp.office365.com]:587` instead.)

Next, add client account details to the SASL password database in
`/etc/postfix/sasl_passwd`:

```
[smtp.gmail.com]:587 username@domain.com:/etc/tokens/username@domain.com
```

(For Outlook, replace `[smtp.gmail.com]:587` with `[smtp.office365.com]:587`.)

The path specified above tells sasl-xoauth2 where to find tokens for the account
"username@domain.com" (but see [A Note on chroot](#a-note-on-chroot) below).

Finally, regenerate the SASL password database:

```
$ sudo postmap /etc/postfix/sasl_passwd
```

#### A Note on chroot

Check if chroot is enabled:

```
$ grep -E '^(smtp|.*chroot)' /etc/postfix/master.cf
# service type  private unpriv  chroot  wakeup  maxproc command + args
smtp      inet  n       -       y       -       -       smtpd
smtp      unix  -       -       y       -       -       smtp
```

In this example `master.cf`, chroot is in fact enabled for the Postfix smtp
process. As a result, the token path specified above will be interpreted at
runtime relative to the Postfix root (`/var/spool/postfix`, usually).

This means that **even though the path in `/etc/postfix/sasl_passwd` is
(and should be) `/etc/tokens/username@domain.com`**, at runtime Postfix will
attempt to read from `/var/spool/postfix/etc/tokens/username@domain.com`.

Additionally, if you see an error message similar to the following, you may need
to copy over root CA certificates for SSL to work within sasl-xoauth2:

```
TokenStore::Refresh: http error: error setting certificate verify locations: ...
```

To copy certificates manually, assuming the Postfix root is
`/var/spool/postfix`:

```
$ sudo mkdir -p /var/spool/postfix/etc/ssl/certs
$ sudo cp /etc/ssl/certs/ca-certificates.crt /var/spool/postfix/etc/ssl/certs/ca-certificates.crt
```

The Debian and Ubuntu packages install a script that is automatically run by
`update-ca-certificates` to ensure the certificates are copied whenever the
system certificates are updated:
`/etc/ca-certificates/update.d/postfix-sasl-xoauth2`. It is also run when
the package is installed.

#### A Note on postmulti

[@jamenlang](https://github.com/jamenlang) has provided a [very helpful
tutorial](https://github.com/jamenlang/sasl-xoauth2-1/wiki/Setting-up-postmulti-with-multiple-xoauth2-relays)
on setting up `postmulti` with sasl-xoauth2.

### Gmail Configuration

#### Client Credentials

Visit the [Google API Console](https://console.developers.google.com/) to obtain
OAuth 2 credentials (a client ID and client secret) for a "Desktop app"
application type.

Store the client ID and secret in `/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "client ID goes here",
  "client_secret": "client secret goes here"
}
```

We'll also need these credentials in the next step.

#### Initial Access Token

Use the [Gmail OAuth2 developer tools](https://github.com/google/gmail-oauth2-tools/)
to obtain an OAuth token by following the [Creating and Authorizing an OAuth
Token](https://github.com/google/gmail-oauth2-tools/wiki/OAuth2DotPyRunThrough#creating-and-authorizing-an-oauth-token)
instructions.

Save the resulting tokens in the file specified in `/etc/postfix/sasl_passwd`.
In our example that file will be either `/etc/tokens/username@domain.com` or
`/var/spool/postfix/etc/tokens/username@domain.com` (see [A Note on
chroot](#a-note-on-chroot)):

```json
{
  "access_token" : "access token goes here",
  "expiry" : "0",
  "refresh_token" : "refresh token goes here"
}
```

In my configuration, chroot is enabled and so even though
`/etc/postfix/sasl_passwd` specifies `/etc/tokens/username@domain.com`,
my token file is `/var/spool/postfix/etc/tokens/username@domain.com`.

It may be necessary to adjust permissions on the token file so that Postfix (or,
more accurately, sasl-xoauth2 running as the Postfix user) can update it:

```
$ sudo chown -R postfix:postfix /etc/tokens
```

or:

```
$ sudo chown -R postfix:postfix /var/spool/postfix/etc/tokens
```

Skip to [restart Postfix](#restart-postfix) below.

### Outlook/Office 365 Configuration

#### Client Credentials

Follow [Microsoft's instructions to register an
application](https://docs.microsoft.com/en-us/azure/active-directory/develop/quickstart-register-app#register-an-application).
Use any name you like (it doesn't have to be "sasl-xoauth2"). Under "Platform
configurations", add a native-client redirect URI for mobile/desktop
applications: `https://login.microsoftonline.com/common/oauth2/nativeclient`.
Then, add API permissions for `SMTP.Send`.

Store the "application (client) ID" (which you'll find in the "Overview" page
for the application you registered with Azure) in `/etc/sasl-xoauth2.conf`.
Leave `client_secret` blank. Additionally, override the token endpoint (which
points to Gmail by default):

```json
{
  "client_id": "client ID goes here",
  "client_secret": "",
  "token_endpoint": "https://login.microsoftonline.com/consumers/oauth2/v2.0/token"
}
```

We'll also need these credentials in the next step.

#### A Note on Token Endpoints

The endpoint above
(`https://login.microsoftonline.com/consumers/oauth2/v2.0/token`) is suitable
for use with consumer Outlook accounts. For other types of accounts it may be
necessary to replace `consumers` with `common`, `organizations`, or a specific
tenant ID. See [Microsoft's OAuth protocol
documentation](https://docs.microsoft.com/en-us/azure/active-directory/develop/active-directory-v2-protocols#endpoints)
for more on this.

#### Initial Access Token

The sasl-xoauth2
[repository](https://github.com/tarickb/sasl-xoauth2/blob/master/scripts/get-initial-outlook-tokens.py)
and pre-built packages include a script to assist in the generation of Microsoft
OAuth tokens. Run the script as follows:

```shell
$ python3 /usr/share/sasl-xoauth2/get-initial-outlook-tokens.py --client_id=CLIENT_ID_FROM_SASL_XOAUTH2_CONF PATH_TO_TOKENS_FILE
Please visit the following link in a web browser, then paste the resulting URL:

https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize?client_id=REDACTED&response_type=code&redirect_uri=https%3A//login.microsoftonline.com/common/oauth2/nativeclient&response_mode=query&scope=openid%20offline_access%20https%3A//outlook.office.com/SMTP.Send

Resulting URL: 
```

If using a tenant other than `consumers`, pass `--tenant=common`,
`--tenant=organizations`, or `--tenant=TENANT_ID`. The client ID will be the
same one written to `/etc/sasl-xoauth2.conf`. And `PATH_TO_TOKENS_FILE` will be
the file specified in `/etc/postfix/sasl_passwd`. In our example that file will
be either `/etc/tokens/username@domain.com` or
`/var/spool/postfix/etc/tokens/username@domain.com` (see [A Note on
chroot](#a-note-on-chroot)).

Visit the link in a browser and accept the various prompts. After authorizing
the application you will be redirected to a blank page. This is expected--copy
the URL of the blank page back into the terminal where you're running the
script, and the script will extract the URL components needed to obtain initial
tokens:

```
Resulting URL: https://login.microsoftonline.com/common/oauth2/nativeclient?code=REDACTED
Tokens written to PATH_TO_TOKENS_FILE.
```

It may be necessary to adjust permissions on the resulting token file so that
Postfix (or, more accurately, sasl-xoauth2 running as the Postfix user) can
update it:

```
$ sudo chown -R postfix:postfix /etc/tokens
```

or:

```
$ sudo chown -R postfix:postfix /var/spool/postfix/etc/tokens
```

#### Further Reading

The following references were useful while developing, testing, and debugging
Outlook support:

* [Authenticate an IMAP, POP or SMTP connection using OAuth](https://docs.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth)
* [Microsoft identity platform and OAuth 2.0 authorization code flow](https://docs.microsoft.com/en-us/azure/active-directory/develop/v2-oauth2-auth-code-flow)
* [Microsoft identity platform and OAuth 2.0 Resource Owner Password Credentials](https://docs.microsoft.com/en-us/azure/active-directory/develop/v2-oauth-ropc)

### Restart Postfix

```
$ service postfix restart
```

## Using Multiple Mail Providers Simultaneously

One instance of sasl-xoauth2 may provide tokens for different mail providers,
but each provider will require its own client ID, client secret, and token
endpoint. In this case, each of these may be set in the token file rather than
in `/etc/sasl-xoauth2.conf`. Set them when setting the initial access token:

```json
{
  "access_token" : "access token goes here",
  "client_id": "client ID goes here",
  "client_secret": "client secret goes here, if required",
  "token_endpoint": "token endpoint goes here, for non-Gmail",
  "expiry" : "0",
  "refresh_token" : "refresh token goes here"
}
```

## Debugging

By default, sasl-xoauth2 will write to syslog if authentication fails. To
disable this, set `log_to_syslog_on_failure` to `no` in
`/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "client ID goes here",
  "client_secret": "client secret goes here",
  "log_to_syslog_on_failure": "no"
}
```

Conversely, to get more verbose logging when authentication fails, set
`log_full_trace_on_failure` to `yes`.

If Postfix complains about not finding a SASL mechanism (along the lines of
`warning: SASL authentication failure: No worthy mechs found`), it's possible
that either `make install` or the pre-built package put libsasl-xoauth2.so in
the wrong directory.
