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

# OPTIONS

The top-level JSON object can contain the following keys:

`client_id`

: describe `client_id` here

`client_secret`

: describe `client_secret` here

`token_endpoint`

: describe `token_endpoint` here

`proxy`

: describe `proxy` here

`log_to_syslog_on_failure`

: describe `log_to_syslog_on_failure` here

# TOKEN FILE

In addition to this file, `sasl-xoauth2` relies on a "token file" which it updates independently.
The token file is also JSON-formatted.
The contents of this token file MAY contain values for the keys described above.
If they do, the value in the token file overrides the value in the main configuration file.

This makes it possible to use the same installation of `sasl-xoauth2` to connect to two different providers simultaneously.

# BUGS

Please report improvements in this documentation upstream at https://github.com/tarickb/sasl-xoauth2/issues

# SEE ALSO

sasl-xoauth2-tool(1)
