Server stub using cpp-httplib (HTTPS)

Requirements:
- Download `httplib.h` to `src/lib/httplib.h` (script: `scripts/fetch_cpp_httplib.ps1`).
- Build with OpenSSL; define `CPPHTTPLIB_OPENSSL_SUPPORT` and link OpenSSL libs.
- Provide PEM-formatted server certificate and private key when running the server.

Run (example):
- Build server.exe for x64.
- `server.exe path\\to\\server.crt path\\to\\server.key`

The server listens on 127.0.0.1:8443 and prints parsed HTTP requests to stdout.
