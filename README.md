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
# Need msal for sasl-xoauth2-tool:
$ sudo pip3 install msal
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

## Pre-Built Packages for RHEL/Fedora

(Thank you [@augustus-p](https://github.com/augustus-p) for confirming that
this works!)

Add the [sasl-xoauth2 Copr
repository](https://copr.fedorainfracloud.org/coprs/jjelen/sasl-xoauth2/):

```
$ sudo dnf copr enable jjelen/sasl-xoauth2
```

Install the plugin:

```
$ sudo dnf install sasl-xoauth2
```

### A Note on SELinux

If SELinux is enabled, you may find that authentication is failing. This is
likely because the sasl-xoauth2 plugin, running within the Postfix `smtp`
process, is unable to read, write, or create a new token file. If in doubt,
check your SELinux audit logs.

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

You can check the effective value by calling `pluginviewer -c` (on Debian/Ubuntu it’s installed as `/usr/sbin/saslpluginviewer` in the `sasl2-bin` package); look for
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

#### SSL/TLS Certificates

If you see an error message similar to the following, you may need to copy over
root CA certificates for the TLS handshake to work within sasl-xoauth2:

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
`/etc/ca-certificates/update.d/postfix-sasl-xoauth2-update-ca-certs`. It is also
run when the package is installed.

sasl-xoauth2 also provides two configuration variables, `ca_bundle_file` and
`ca_certs_dir`, that may be used to manually configure where the SSL/TLS
libraries will look for a CA certificate bundle (for `ca_bundle_file`) or a set
of CA certificates (for `ca_certs_dir`). Specify one or the other, but not both.

#### A Note on postmulti

[@jamenlang](https://github.com/jamenlang) has provided a [very helpful
tutorial](https://github.com/jamenlang/sasl-xoauth2-1/wiki/Setting-up-postmulti-with-multiple-xoauth2-relays)
on setting up `postmulti` with sasl-xoauth2.

### Gmail Configuration

From a new account, Google requires several steps to enable access.
Once you are logged into your Gmail account in the browser, all these steps happen at the [Google Cloud Platform console](https://console.cloud.google.com/).

#### Basic Account Setup

- Select an existing project, or add a Project if you don't have one yet (it can be any name)

- Set up "OAuth Consent Screen" for the project

  - If this is an "External" app, make sure the "Publishing status" of the app is set to "In production" (as opposed to "Testing"), otherwise the token [will be revoked after 7 days](https://github.com/tarickb/sasl-xoauth2/issues/29).
    - You can ignore any requests to "verify" your app. The warnings shown in the console are misleading. You don't actually need to go through verification.

#### Client Credentials

From the [Google Cloud Platform console](https://console.cloud.google.com/),

- Credentials: Create Credentials: OAuth client ID

  - Application type: Desktop app

  - Choose a memorable name

Store the client ID and secret in `/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "client ID goes here",
  "client_secret": "client secret goes here"
}
```

We'll also need these credentials in the next step.

#### Initial Access Token

The sasl-xoauth2 package includes a script that can assist in the generation of
Gmail OAuth tokens. Run the script as follows:

```shell
$ sasl-xoauth2-tool get-token gmail \
    --client-id=CLIENT_ID_FROM_SASL_XOAUTH2_CONF \
    --client-secret=CLIENT_SECRET_FROM_SASL_XOAUTH2_CONF \
    --scope="https://mail.google.com/" \
    PATH_TO_TOKENS_FILE

Please open this URL in a browser ON THIS HOST:

https://accounts.google.com/o/oauth2/auth?client_id=&scope=&response_type=code&redirect_uri=http%3A%2F%2F127.0.0.1%3A12345%2Foauth2_result
```

(This script must run on the same host that is opening the URL -- it's not
possible to copy the URL and paste it into a browser on another computer. This
is because [recent
changes](https://developers.googleblog.com/2022/02/making-oauth-flows-safer.html)
to the OAuth2 authorization flow require that the browser pass the resulting
authorization code directly to the requesting application. If the Postfix
installation is running on a headless host, simply run the script on a host with
a usable browser then copy the resulting token file over to the headless host.)

Opening the URL and authorizing the application should result in a new token in
`PATH_TO_TOKENS_FILE`, which should be the file specified in
`/etc/postfix/sasl_passwd`.  In our example that file will be either
`/etc/tokens/username@domain.com` or
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

### Outlook/Office 365 Configuration (Device Flow)

As of sasl-xoauth2-0.23, this is the preferred method to authenticate with
Outlook/Office, but the [fallback legacy client
approach](#outlookoffice-365-configuration-legacy-client-deprecated) does still
work (... for now).

#### Client Credentials

Follow [Microsoft's instructions to register an
application](https://docs.microsoft.com/en-us/azure/active-directory/develop/quickstart-register-app#register-an-application),
with some notes:

* Use any name you like (it doesn't have to be "sasl-xoauth2").
* Do **not** add any redirect URIs or set up any platform configurations.
* You **must** toggle "Allow public client flows" to "yes".

Then, add API permissions for `SMTP.Send`:

1. From the app registration "API permissions" page, click "add a permission".
1. Click "Microsoft Graph".
1. Enter "SMTP.Send" in the search box.
1. Expand the `SMTP` permission, then check the `SMTP.Send` checkbox.

Store the "application (client) ID" (which you'll find in the "Overview" page
for the application you registered with Azure) in `/etc/sasl-xoauth2.conf`.
Leave `client_secret` blank. Additionally, explicitly set the token endpoint
(`sasl-xoauth2` points to Gmail's token endpoint by default):

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

The sasl-xoauth2 package includes a script that can assist in the generation of
Microsoft OAuth tokens. Run the script as follows:

```shell
$ sasl-xoauth2-tool get-token outlook \
    --client-id=CLIENT_ID_FROM_SASL_XOAUTH2_CONF \
    --use-device-flow \
    PATH_TO_TOKENS_FILE
To sign in, use a web browser to open the page https://www.microsoft.com/link and enter the code REDACTED to authenticate.
```

If using a tenant other than `consumers`, pass `--tenant=common`,
`--tenant=organizations`, or `--tenant=TENANT_ID`. The client ID will
be the same one written to `/etc/sasl-xoauth2.conf`. And `PATH_TO_TOKENS_FILE`
will be the file specified in `/etc/postfix/sasl_passwd`. In our example that
file will be either `/etc/tokens/username@domain.com` or
`/var/spool/postfix/etc/tokens/username@domain.com` (see [A Note on
chroot](#a-note-on-chroot)).

Visit the link in a browser, enter the code, then accept the various prompts.
After authorizing the application, the tool will write a token to the path
specified.

```
To sign in, use a web browser to open the page https://www.microsoft.com/link and enter the code REDACTED to authenticate.
Acquired token.
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

### Outlook/Office 365 Configuration (Legacy Client) (Deprecated)

#### Client Credentials

Follow [Microsoft's instructions to register an
application](https://docs.microsoft.com/en-us/azure/active-directory/develop/quickstart-register-app#register-an-application).
Use any name you like (it doesn't have to be "sasl-xoauth2"). Under "Platform
configurations", add a native-client redirect URI for mobile/desktop
applications: `https://login.microsoftonline.com/common/oauth2/nativeclient`.
Then, add API permissions for `SMTP.Send`: from the app registration
"API permissions" page, click "add a permission", then "Microsoft Graph", and
from there enter "SMTP.Send" in the search box. Expand the `SMTP` permission,
then check the `SMTP.Send` checkbox.

Store the "application (client) ID" (which you'll find in the "Overview" page
for the application you registered with Azure) in `/etc/sasl-xoauth2.conf`.
Leave `client_secret` blank (but see [A Note on Client
Secrets](#a-note-on-client-secrets) below for non-personal-Outlook-account
situations). Additionally, explicitly set the token endpoint (`sasl-xoauth2`
points to Gmail's token endpoint by default):

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

The sasl-xoauth2 package includes a script that can assist in the generation of
Microsoft OAuth tokens. Run the script as follows:

```shell
$ sasl-xoauth2-tool get-token outlook \
    --client-id=CLIENT_ID_FROM_SASL_XOAUTH2_CONF \
    PATH_TO_TOKENS_FILE
Please visit the following link in a web browser, then paste the resulting URL:

https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize?client_id=REDACTED&response_type=code&redirect_uri=https%3A//login.microsoftonline.com/common/oauth2/nativeclient&response_mode=query&scope=openid%20offline_access%20https%3A//outlook.office.com/SMTP.Send

Resulting URL: 
```

If using a tenant other than `consumers`, pass `--tenant=common`,
`--tenant=organizations`, or `--tenant=TENANT_ID` (and see [A Note on Client
Secrets](#a-note-on-client-secrets), which may be relevant). The client ID will
be the same one written to `/etc/sasl-xoauth2.conf`. And `PATH_TO_TOKENS_FILE`
will be the file specified in `/etc/postfix/sasl_passwd`. In our example that
file will be either `/etc/tokens/username@domain.com` or
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

#### A Note on Client Secrets

Some users have [reported](https://github.com/tarickb/sasl-xoauth2/issues/61)
needing to specify client secrets when requesting access and refresh tokens for
Outlook. This would seem to be the case when registering an application that has
access to "accounts in any organizational directory" (i.e., non-personal
Microsoft accounts). If this applies to you, please specify the client secret in
`/etc/sasl-xoauth2.conf` and on the command line when using `sasl-xoauth2-tool`.

#### Further Reading

The following references were useful while developing, testing, and debugging
Outlook support:

- [Authenticate an IMAP, POP or SMTP connection using OAuth](https://docs.microsoft.com/en-us/exchange/client-developer/legacy-protocols/how-to-authenticate-an-imap-pop-smtp-application-by-using-oauth)
- [Microsoft identity platform and OAuth 2.0 authorization code flow](https://docs.microsoft.com/en-us/azure/active-directory/develop/v2-oauth2-auth-code-flow)
- [Microsoft identity platform and OAuth 2.0 Resource Owner Password Credentials](https://docs.microsoft.com/en-us/azure/active-directory/develop/v2-oauth-ropc)

### Proxy Support

In case the system is behind a corporate web proxy you can configure a proxy
that is used by the curl library when refreshing the token.

```json
{
  "client_id": "client ID goes here",
  "client_secret": "client secret goes here",
  "token_endpoint": "token endpoint goes here",
  "proxy" : "http://proxy:8080"
}
```

For supported proxy schemes please refer to the [curl library documentation](https://curl.se/libcurl/c/CURLOPT_PROXY.html)

### Testing Your Configuration

sasl-xoauth2 provides a tool, `sasl-xoauth2-tool`, that allows the
semi-interative testing of configuration and token files (which is a lot more
useful than parsing log files when trying to figure out why Postfix isn't
delivering mail correctly).

First, test your configuration file:

```
$ sasl-xoauth2-tool test-config --config-file ./bad-config.conf
sasl-xoauth2: Missing required value: client_secret
Config check failed.
$ sasl-xoauth2-tool test-config --config-file ./good-config.conf
Config check passed.
$ sasl-xoauth2-tool test-config --config-file /etc/sasl-xoauth2.conf
Config check passed.
```

(Specifying the path is only required if your configuration file isn't located
in the system-default path.)

Next, test your token file:

```
$ sasl-xoauth2-tool test-token-refresh ./bad-token.json
Config check passed.
2022-09-10 09:18:59: TokenStore::Read: file=./bad-token.json
2022-09-10 09:18:59: TokenStore::Read: refresh=REDACTED
2022-09-10 09:18:59: TokenStore::Refresh: attempt 1
2022-09-10 09:18:59: TokenStore::Refresh: token_endpoint: https://accounts.google.com/o/oauth2/token
2022-09-10 09:18:59: TokenStore::Refresh: request: client_id=REDACTED&client_secret=REDACTED&grant_type=refresh_token&refresh_token=REDACTED
2022-09-10 09:19:00: TokenStore::Refresh: code=400, response={
  "error": "invalid_grant",
  "error_description": "Bad Request"
}
2022-09-10 09:19:00: TokenStore::Refresh: request failed
Token refresh failed.
$ sasl-xoauth2-tool test-token-refresh ./good-token.json
Config check passed.
Token refresh succeeded.
```

(Again, you'll have to specify your configuration file with
`--config-file <config file>` if it isn't located at the system-default path.)

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

## Building

sasl-xoauth2 uses [git-buildpackage](https://github.com/agx/git-buildpackage)
for Debian and Ubuntu builds. The following is mostly intended as a cheat-sheet.

### Creating a New Distribution Branch for an Existing Release

```
$ TARGET_DIST=dist-name # debian, ubuntu, etc.
$ TARGET_DIST_RELEASE=release-name # focal, jammy, etc.
$ RELEASE=0.NN
$ RELEASE_VERSION="$RELEASE-1ubuntu1~${TARGET_DIST_RELEASE}1~ppa1"
$ git clone --no-checkout -o upstream git@github.com:tarickb/sasl-xoauth2.git
$ cd sasl-xoauth2
$ git checkout -b "$TARGET_DIST/$TARGET_DIST_RELEASE" "release-$RELEASE"
$ git checkout "upstream/packaging/$TARGET_DIST" debian/
$ dch --create --package "sasl-xoauth2" --newversion "$RELEASE_VERSION" \
    --distribution "$TARGET_DIST_RELEASE"
$ git add debian/
$ git commit -m \
    "Initial packaging commit for $TARGET_DIST/$TARGET_DIST_RELEASE." debian/
```

### Updating an Existing Release Branch for a New Release

```
$ TARGET_DIST=dist-name # debian, ubuntu, etc.
$ TARGET_DIST_RELEASE=release-name # focal, jammy, etc.
$ RELEASE=0.NN
$ RELEASE_VERSION="$RELEASE-1ubuntu1~${TARGET_DIST_RELEASE}1~ppa1"
$ git fetch upstream
$ git checkout "$TARGET_DIST/$TARGET_DIST_RELEASE"
$ git merge "release-$RELEASE"
$ gbp dch --release --auto --debian-branch="$TARGET_DIST/$TARGET_DIST_RELEASE" \
    -N "$RELEASE_VERSION" --distribution="$TARGET_DIST_RELEASE"
$ git commit -m "Release $RELEASE_VERSION" debian/changelog
```

### Building and Uploading a Release

```
$ TARGET_DIST=dist-name # debian, ubuntu, etc.
$ TARGET_DIST_RELEASE=release-name # focal, jammy, etc.
$ gbp buildpackage --git-debian-branch="$TARGET_DIST/$TARGET_DIST_RELEASE" \
    -S --git-tag
$ dput ppa:sasl-xoauth2/stable build/*.changes
```
