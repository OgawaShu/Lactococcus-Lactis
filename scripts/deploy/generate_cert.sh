#!/usr/bin/env bash
set -e
CERT_PATH=${1:-src/third_party/certs/server.crt}
KEY_PATH=${2:-src/third_party/certs/server.key}

mkdir -p "$(dirname "$CERT_PATH")"

if ! command -v openssl >/dev/null 2>&1; then
  echo "OpenSSL CLI not found. Attempting to install via apt-get or yum..."
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update && sudo apt-get install -y openssl
  elif command -v yum >/dev/null 2>&1; then
    sudo yum install -y openssl
  else
    echo "No supported package manager found. Please install openssl manually." >&2
    exit 1
  fi
fi

openssl req -x509 -newkey rsa:2048 -days 365 -nodes -keyout "$KEY_PATH" -out "$CERT_PATH" -subj "/CN=localhost"

echo "Generated cert: $CERT_PATH"
echo "Generated key: $KEY_PATH"
