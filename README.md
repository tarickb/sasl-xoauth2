## sasl-xoauth2

XOAUTH2 Simple Authentication and Security Layer (SASL) plugin for Postfix SMTP relay with Gmail and Office 365.

## Installation

### Ubuntu

```bash
sudo add-apt-repository ppa:sasl-xoauth2/stable
sudo apt-get update
sudo apt-get install sasl-xoauth2
```

### RHEL/EPEL/Fedora

Enable EPEL: [https://docs.fedoraproject.org/en-US/epel/](https://docs.fedoraproject.org/en-US/epel/)

```bash
sudo dnf install sasl-xoauth2
```

For older Fedora:

```bash
sudo dnf copr enable jjelen/sasl-xoauth2
```

### Build from Source

```bash
mkdir build && cd build && cmake ..
make
sudo make install
sudo pip3 install msal
```

Debian/Ubuntu with system-packaged Postfix:

```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_SYSCONFDIR=/etc
make
sudo make install
sudo pip3 install msal
```

## Basic Setup Guide for Ubuntu

For other linux distributions the paths may vary.

### Step 1: Configure Postfix

Add to `/etc/postfix/main.cf`:

```
smtp_sasl_auth_enable = yes
smtp_sasl_password_maps = hash:/etc/postfix/sasl_passwd
smtp_sasl_security_options =
smtp_sasl_mechanism_filter = xoauth2
smtp_tls_security_level = encrypt
smtp_tls_CAfile = /etc/ssl/certs/ca-certificates.crt
smtp_tls_session_cache_database = btree:${data_directory}/smtp_scache
```

For Gmail, add:

```
relayhost = [smtp.gmail.com]:587
```

For Outlook, add:

```
relayhost = [smtp.office365.com]:587
```

### Step 2: Check Chroot Status

```bash
grep -E '^(smtp|.*chroot)' /etc/postfix/master.cf
```

Note whether chroot is enabled `(y)` or disabled `(n)` - this determines **some** of the paths in later steps (which will be indicated with a comment).

### Step 3: Configure SASL Password Map

Add to `/etc/postfix/sasl_passwd`:

For Gmail:
```
[smtp.gmail.com]:587 username@domain.com:/etc/tokens/username@domain.com
# Use smtp.office365.com for Outlook
```

Generate database:

```bash
sudo postmap /etc/postfix/sasl_passwd
```

### Step 4: Setup OAuth Credentials

#### For Gmail

Visit [Google Cloud Platform console](https://console.cloud.google.com/).

Create or select a project.

Configure OAuth Consent Screen:
- Create a client with application type: Desktop app
- Copy down the client secret and client id
- Add a scope for https://mail.google.com (under Manually add scopes section -> add to table)
- Set Publishing status to "In production" (prevents 7-day token expiration) (if you have google workspace this is not necessary if you use internal option)

Save credentials to `/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "your_client_id_here",
  "client_secret": "your_client_secret_here"
}
```

Note: 


#### For Outlook (Device Flow)

Follow [Microsoft's app registration instructions](https://docs.microsoft.com/en-us/azure/active-directory/develop/quickstart-register-app#register-an-application):

- Use any name
- No redirect URIs or platform configurations
- Toggle "Allow public client flows" to "yes"
- Select appropriate account type (consumer vs organizational)

Add API permissions:
1. App registration → API permissions → Add permission
2. Microsoft Graph
3. Search "SMTP.Send"
4. Expand SMTP → Check SMTP.Send

Save to `/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "your_client_id_here",
  "client_secret": "",
  "token_endpoint": "https://login.microsoftonline.com/consumers/oauth2/v2.0/token"
}
```

For organizational accounts, replace `consumers` with `common`, `organizations`, or tenant ID.

For HVE endpoint (smtp-hve.office365.com), add `"refresh_window": 600`.

#### For Outlook (Legacy Client - Deprecated)

Follow [Microsoft's app registration instructions](https://docs.microsoft.com/en-us/azure/active-directory/develop/quickstart-register-app#register-an-application):

- Select appropriate account type
- Add redirect URI: `https://login.microsoftonline.com/common/oauth2/nativeclient`
- Add API permissions for SMTP.Send

Save to `/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "your_client_id_here",
  "client_secret": "",
  "token_endpoint": "https://login.microsoftonline.com/consumers/oauth2/v2.0/token"
}
```

For organizational accounts, replace `consumers` with `common`, `organizations`, or tenant ID.

For organizational directory accounts, add `client_secret`.

### Step 5: Create Token Directory

```bash
sudo mkdir -p /var/spool/postfix/etc/tokens  # Use /etc/tokens if chroot disabled
```

### Step 6: Generate Initial Token

#### For Gmail

```bash
# Use /etc/tokens/<username@domain.com> if chroot disabled
sasl-xoauth2-tool get-token gmail \
    /var/spool/postfix/etc/tokens/<username@domain.com> \  
    --client-id=YOUR_CLIENT_ID \
    --client-secret=YOUR_CLIENT_SECRET \
    --scope="https://mail.google.com/"
```

Open the displayed URL in a browser and authorize. 

Note: The Gmail OAuth flow requires a local server on localhost to receive the authorization callback. If setting up on a remote/headless server via SSH, you can either:
1. Run `sasl-xoauth2-tool` on your local machine and copy the resulting token file to the server, or
2. Use SSH port forwarding to access the localhost server from your local browser

#### For Outlook (Device Flow)

```bash
# Use /etc/tokens/<username@domain.com> if chroot disabled
sasl-xoauth2-tool get-token outlook \
    /var/spool/postfix/etc/tokens/<username@domain.com> \  
    --client-id=YOUR_CLIENT_ID \
    --use-device-flow
```

For non-consumer tenants, add `--tenant=common`, `--tenant=organizations`, or `--tenant=TENANT_ID`.

Visit link in browser, enter code, authorize.

#### For Outlook (Legacy Client)

```bash
# Use /etc/tokens/<username@domain.com> if chroot disabled
sasl-xoauth2-tool get-token outlook \
    /var/spool/postfix/etc/tokens/<username@domain.com> \  
    --client-id=YOUR_CLIENT_ID
```

For non-consumer tenants, add `--tenant=common`, `--tenant=organizations`, or `--tenant=TENANT_ID`.

Visit link, authorize, copy resulting URL back to terminal.

### Step 7: Set Permissions

```bash
# Use /etc/tokens if chroot disabled
sudo chown -R postfix:postfix /var/spool/postfix/etc/tokens  
```

### Step 8: Test Configuration

```bash
sasl-xoauth2-tool test-config --config-file /etc/sasl-xoauth2.conf
```

Test token refresh:

```bash
# Use /etc/tokens/<username@domain.com> if chroot disabled
sasl-xoauth2-tool test-token-refresh /var/spool/postfix/etc/tokens/<username@domain.com>  
```

### Step 9: Restart Postfix

```bash
sudo service postfix restart
```

## Multiple Providers or Users

Set provider-specific values in token file instead of `/etc/sasl-xoauth2.conf`:

```json
{
  "access_token": "access_token_here",
  "client_id": "client_id_here",
  "client_secret": "client_secret_here_if_required",
  "token_endpoint": "token_endpoint_for_non_gmail",
  "expiry": "0",
  "refresh_token": "refresh_token_here",
  "user": "username_if_needed"
}
```

Use `--overwrite-existing-token` with `sasl-xoauth2-tool` to preserve these fields.

## Proxy Support

Add to `/etc/sasl-xoauth2.conf`:

```json
{
  "client_id": "client_id_here",
  "client_secret": "client_secret_here",
  "token_endpoint": "token_endpoint_here",
  "proxy": "http://proxy:8080"
}
```

See [curl proxy documentation](https://curl.se/libcurl/c/CURLOPT_PROXY.html) for supported schemes.

## Troubleshooting

This package includes a manpage: `man sasl-xoauth2.conf` (sourced from `docs/sasl-xoauth2.conf.5.md`) that specifies all the configuration options available

### SSL/TLS Errors

If seeing certificate errors, you may need to copy over root CA certificates for the TLS handshake to work within sasl-xoauth2:

```bash
# you may need to change /var/spool/postfix based on your linux distro
sudo mkdir -p /var/spool/postfix/etc/ssl/certs 
sudo cp /etc/ssl/certs/ca-certificates.crt /var/spool/postfix/etc/ssl/certs/ca-certificates.crt
```

SSL/TLS errors can happen when certificates are missing in chroot installations.

The Debian package includes `/etc/ca-certificates/update.d/postfix-sasl-xoauth2-update-ca-certs`, which automatically copies certificates to the chroot environment whenever system certificates are updated via `update-ca-certificates`.

Additionally, sasl-xoauth2 supports manual CA certificate configuration via `/etc/sasl-xoauth2.conf`:

- `ca_bundle_file` - Path to a single CA certificate bundle file
- `ca_certs_dir` - Path to a directory containing CA certificates

Use only one of these options (not both), depending on your system's certificate storage format.

### Test Configuration

```bash
sasl-xoauth2-tool test-config --config-file /etc/sasl-xoauth2.conf
```

### Test Token Refresh

```bash
sasl-xoauth2-tool test-token-refresh PATH_TO_TOKEN_FILE
```

Add `--config-file /etc/sasl-xoauth2.conf` if config not in default location.

### Logging Options

Add to `/etc/sasl-xoauth2.conf`:

Disable logging on failure:
```json
{
  "log_to_syslog_on_failure": "no"
}
```

Verbose logging on failure:
```json
{
  "log_full_trace_on_failure": "yes"
}
```

Always log:
```json
{
  "always_log_to_syslog": "yes"
}
```

### Check SASL Mechanisms

```bash
/usr/sbin/saslpluginviewer -c
```

Look for XOAUTH2 with SSF: 60.

### SELinux

Check SELinux audit logs if authentication fails with SELinux enabled.

### Postfix Logging

Increase Postfix logging per [Postfix debug instructions](https://www.postfix.org/DEBUG_README.html#verbose).

**Disclaimer:** Not an officially supported Google product.