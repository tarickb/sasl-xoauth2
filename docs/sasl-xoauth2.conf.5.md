% sasl-xoauth2.conf(5) | File Formats Manual

# NAME

/etc/sasl-xoauth2.conf - configuration file for sasl-xoauth2

# DESCRIPTION

This file contains static, administrator-defined information needed for XOAUTH2 SASL authentication.

It uses a JSON format to define variables needed to complete XOAUTH2 configuration. 

A minimal configuration file looks like:

```json
{
  "client_id": "CLIENT_ID_GOES_HERE",
  "client_secret": "CLIENT_SECRET_GOES_HERE"
}
```

See the full README for guidance on initial configuration:
https://github.com/tarickb/sasl-xoauth2

# OPTIONS

The top-level JSON object can contain the following keys:

`client_id`

: identifies this client for OAuth 2 token requests

`client_secret`

: authenticates this client for OAuth 2 token requests; world-readable by default (but see below to place this in token files instead)

`scope`

: if the client credentials grant is used, we need a scope. Specially for office365.com where the default scope should be: `https://outlook.office365.com/.default` 

`always_log_to_syslog`

: always write plugin log messages to syslog, even for successful runs; may contain tokens/secrets (defaults to "no")

`log_to_syslog_on_failure`

: log to syslog if XOAUTH2 flow fails (defaults to "yes")

`log_full_trace_on_failure`

: log a full trace to syslog if XOAUTH2 flow fails; may contain tokens/secrets (defaults to "no")

`token_endpoint`

: URL to use when requesting tokens; defaults to Google, must be overridden for use with Microsoft/Outlook

`proxy`

: if set, HTTP requests will be proxied through this server

`ca_bundle_file`

: if set, overrides CURL's default certificate-authority bundle file. **Remember curl expect a new line after the last certificate in the bundle**.

`ca_certs_dir`

: if set, overrides CURL's default certificate-authority directory

`refresh_window`

: if set, overrides the default 10 second refresh window with the specified time in seconds (integer)

`manage_token_externally`

: if set, especially in the token file, the plugin takes the token as is

`use_client_credentials`

: if set, the client credentials grant flow is used, instead of the refresh_token flow

# TOKEN FILE

In addition to this file, `sasl-xoauth2` relies on a "token file" which it updates independently.
The token file is also JSON-formatted.
The contents of this token file MAY contain values for the keys described above (except for the logging-related keys).
If they do, the value in the token file overrides the value in the main configuration file.

This makes it possible to use the same installation of `sasl-xoauth2` to connect to two different providers simultaneously.
This also has the benefit of providing storage for client secrets that is not world-readable.

# BUGS

Please report improvements in this documentation upstream at https://github.com/tarickb/sasl-xoauth2/issues

# SEE ALSO

sasl-xoauth2-tool(1)
