Overview of transport threading and classes

This directory contains a small thread-based transport implementation used by the server.

Key classes

- `HttpsTransportServer` (src/server/transport/HttpsTransportServer.h/.cpp)
  - High-level entrypoint used by `ServerApplication`.
  - Manages an acceptor and a set of worker threads (via helper classes).
  - Provides queue-based APIs for the main application thread to receive parsed HTTP requests (via `WaitAndPopHttpText`) and to enqueue responses targeted at a specific connection (`EnqueueResponse`).

- `Acceptor` (src/server/transport/Acceptor.h/.cpp)
  - Single dedicated thread which performs `accept()` on the listening socket.
  - When a connection is accepted it calls `HttpsTransportServer::EnqueueConnection()` to hand the socket to workers.
  - Keeps accept logic isolated from per-connection processing.

- `Worker` (src/server/transport/Worker.h/.cpp)
  - Each worker thread pops a connection id from `HttpsTransportServer::PopConnection()` and owns the lifecycle of that connection.
  - Worker reads from the socket (or from an OpenSSL `SSL*` wrapper when TLS is enabled), accumulates until a full HTTP request is available (header + body), parses it into `HttpTextRequest` and pushes it to the main request queue with `PushRequest()`.
  - After pushing the request, a worker checks for a pending response submitted by the business thread. If a response exists for that connection the worker will send it (using `SendHttpResponse`) and then close the connection.
  - Worker also owns creation and destruction of per-connection `SSL*` objects.

Data flow

1. Accept loop accepts a socket and enqueues it to the connection queue.
2. One worker picks the socket and performs per-connection processing (TLS accept, read/parse requests, push requests to the main queue).
3. Main (business) thread pulls a `HttpTextRequest` from the queue, does application logic and then calls `EnqueueResponse(connection, response)` to tell the worker to reply.
4. The worker that owns the connection sees the pending response, sends it on the socket, and closes the connection.

TLS support

- TLS is optional and enabled when OpenSSL is found during CMake. When TLS is enabled the worker wraps the socket in an `SSL*` and uses `SSL_read` / `SSL_write` for I/O.
- Certificate/key are loaded into a shared `SSL_CTX*` during server startup.

Notes about conditional compilation

- The code aims to isolate platform-specific and OpenSSL-specific code into small locations (SocketUtils.h and a couple of blocks in Worker/Acceptor). This reduces #ifdef proliferation in the high-level logic.
- To enable TLS fully on the build machine ensure OpenSSL is discoverable by CMake (or use vcpkg). The repository includes helper scripts to generate a self-signed certificate for development (`scripts/deploy/generate_cert.*`).

Configuration

- Server-wide options are provided via `ServerConfig` (e.g. thread pool size, keep-alive timeout).

If you want further refactors (per-connection state class, connection timeouts, per-connection response queues), tell me and I will add them step-by-step.