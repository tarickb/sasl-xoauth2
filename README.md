# sasl-xoauth2

## Background

sasl-xoauth2 is a SASL plugin that enables client-side use of OAuth 2.0.

## Building from Source

Fetch the sources, then:

$ mkdir build && cd build && cmake ..
$ make
$ sudo make install

## Pre-Built Packages for Ubuntu

Add the [sasl-xoauth2 PPA](https://launchpad.net/~sasl-xoauth2/+archive/ubuntu/stable):

$ sudo add-apt-repository ppa:sasl-xoauth2/stable
$ sudo apt-get update

Install the plugin:

$ sudo apt-get install sasl-xoauth2
