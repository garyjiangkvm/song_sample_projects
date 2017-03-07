#!/bin/bash
set -e

curl https://bootstrap.pypa.io/get-pip.py | python
pip install simplejson requests docker-py flask
