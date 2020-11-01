# sasl-xoauth2

## Disclaimer

This is not an officially supported Google product.

## Background

sasl-xoauth2 is a SASL plugin that enables client-side use of OAuth 2.0.

## Building from Source

Fetch the sources, then:

```
$ mkdir build && cd build && cmake ..
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

Next, add client Gmail account details to the SASL password database in
`/etc/postfix/sasl_passwd`:

```
[smtp.gmail.com]:587 username@domain.com:/etc/tokens/username@domain.com
```

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

This means that *even though the path in `/etc/postfix/sasl_passwd` is
(and should be) `/etc/tokens/username@domain.com`*, at runtime Postfix will
attempt to read from `/var/spool/postfix/etc/tokens/username@domain.com`.

Additionally, if you see an error message similar to the following, you may need
to manually copy over root CA certificates for SSL to work within sasl-xoauth2:

```
TokenStore::Refresh: http error: error setting certificate verify locations: ...
```

To copy certificates manually, assuming the Postfix root is
`/var/spool/postfix`:

```
$ sudo mkdir -p /var/spool/postfix/etc/ssl/certs
$ sudo cp /etc/ssl/certs/ca-certificates.crt /var/spool/postfix/etc/ssl/certs/ca-certificates.crt
```

### Client Credentials

Visit the [Google API Console](https://console.developers.google.com/) to obtain
OAuth 2 credentials (a client ID and client secret) for an "Installed
application" application type.

Store the client ID and secret in `/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "client ID goes here",
  "client_secret": "client secret goes here"
}
```

We'll also need these credentials in the next step.

### Obtain Initial Access Token

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

### Restart Postfix

```
$ service postfix restart
```

## Debugging

By default, sasl-xoauth2 will write to syslog if authentication fails. To
disable this, set `log_to_syslog_on_failure` to `no` in `/etc/sasl-xoauth2.conf`:

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
