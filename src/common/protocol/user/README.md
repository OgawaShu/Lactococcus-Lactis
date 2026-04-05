# User protocol

This document explains the messaging flow, authentication semantics, and developer notes for messages under `protocol::user`.

Overview
- Main messages:
  - `BeginRegisterUserRequest` / `BeginRegisterUserResponse`: used to start registration. The server includes a `registrationChallenge` in the response for the client to sign.
  - `RegisterUserRequest` / `RegisterUserResponse`: client submits registration results (including public key and proof).

Authentication and the `auth` field
- `MessageHeader` contains an `auth` field (in memory it is recommended to keep the challenge as binary; transport uses a Base64 encoded string).
- Semantic conventions:
  - `auth == "0"` means the message does not require the client to sign a reply with its private key (no verification required).
  - `auth != "0"` means the server requires the client to sign a response; typically the server also issues an `auth_challenge` (32 bytes of CSPRNG, Base64 encoded).

Signing and verification guidelines (recommendations)
- Signature algorithm: Ed25519 is recommended (or ECDSA-SHA256, as decided by the project).
- Signature scope: the client should sign the deterministically serialized data below:
  - `canonical(header)` (excluding the `auth_signature` field) + `"|"` + `body` (or the SHA256 digest of the body).
- Replay protection: the server must use each challenge only once or combine it with an `auth_ts`/nonce and validate within a short time window.
- Transport: send challenge and signature as Base64 strings in JSON. Do not log private keys or plaintext challenge/signatures.

Message handling flow (recommended implementation)
1. The server decides whether verification is required:
   - If not required: set `header.auth = "0"` and send the message.
   - If required: generate a random challenge, set `header.auth != "0"` and place the Base64 challenge in the header, then send the message.
2. Client receives the message:
   - If `auth == "0"`: process per protocol and keep `auth` as "0" in the response (or omit a signature).
   - If signing is required: sign the agreed data per the convention, put the Base64 signature into the response header (e.g. `auth_signature` or merged into `auth` structure), and send it.
3. Server receives the client response: verify the signature using the server-stored public key; if verification passes, perform sensitive operations; if it fails, reject and log.

Implementation notes and security requirements
- Keep the challenge in memory as `std::array<uint8_t,32>` (or `std::vector<uint8_t>`); encode to Base64 when serializing.
- After handling sensitive data, securely zero buffers (e.g. `explicit_bzero` / `sodium_memzero`).
- Encapsulate signing/verification logic into helpers (based on libsodium / OpenSSL). Do not reimplement low-level crypto primitives.
- If the server supports deferred verification (enqueue and verify asynchronously), ensure no modifying or sensitive operation is performed before verification succeeds; mark messages as "unverified" and consume them only after verification.

Developer tips
- The codebase currently has simple hand-written `ToJson`/`FromJson` implementations. Consider migrating to a mature JSON library (e.g. `nlohmann/json`) for robustness.
- When implementing cross-module auth, keep header field naming consistent (for example use `auth_challenge_b64` and `auth_signature_b64`).

If you want me to add a minimal Base64 helper, `ToJson`/`FromJson` helpers, or an example signing/verification implementation, tell me which crypto library to use (libsodium / OpenSSL / custom).

Registration flow (recommended implementation)
1. Client issues a registration start request (`BeginRegisterUserRequest`) over the same HTTPS connection to the server.
2. The server generates a unique `userId` (e.g. UUIDv4) for this registration and generates a random `auth_challenge` (recommended 32 bytes of CSPRNG).
3. The server sends `BeginRegisterUserResponse` back to the client. The header contains an `auth` indicator (non-"0") and the `auth_challenge` (Base64), and the response body includes the `userId`. This response and the client's subsequent reply must occur on the same HTTPS connection (connection-level context) to prevent a man-in-the-middle from replacing the challenge.
4. The client generates a key pair (recommended Ed25519 or ECDSA), signs the server-sent `auth_challenge` with the private key, then constructs `RegisterUserRequest` containing `userId`, the public key (Base64/PEM) and `registrationProof` (the signature Base64). The client sends this along with the `auth`/`auth_challenge` (optional) over the same HTTPS connection.
5. When the server receives `RegisterUserRequest`:
   - Verify that the `userId` matches the previously issued challenge (for example using associated temporary storage or connection-scoped mapping).
   - Use the received public key to verify the `registrationProof` (signature over the challenge or challenge+metadata).
   - If verification succeeds, persist `userId` and public key and return success; if verification fails, reject and log.

Security analysis (weaknesses and mitigations)
- The core assumption of this flow is that the server-issued challenge and the client's signature must complete on the same protected channel (the same HTTPS connection) to prevent a man-in-the-middle (MITM) from substituting the challenge or performing replay attacks.
- Potential weaknesses and mitigations:
  - MITM replacing the challenge: if the client cannot be sure the challenge came from the genuine server, an attacker can replace it and obtain a forgeable signature. Mitigation: ensure TLS server certificate validation (CA verification) and bind the application-layer challenge to the connection context where possible (short-lived session, TLS channel binding, or require the client to verify a server certificate fingerprint).
  - Replay attacks: attackers replay prior registration messages or signatures. Mitigation: challenges must be single-use and the server should enforce one-time consumption; include timestamps in challenges and reject expired ones, or keep a server-side list of used challenges until registration completes.
  - Connection hijacking for subsequent requests: even if registration completes on the same connection, if the connection is hijacked or TLS is terminated by a proxy, an attacker could intercept or modify messages. Mitigation: prefer end-to-end TLS (and validate certificates); consider client certificates (mTLS) or record TLS channel binding values at registration and bind them to subsequent sessions.
  - Private key generation and storage risk: if the client's environment is compromised, private keys may be stolen. Mitigation: recommend secure elements or OS key stores; document that the client must store keys securely and never send private keys to the server.
  - Race / TOCTOU: if the server does not atomically bind the challenge to the `userId` between issuance and verification, concurrency may cause interference. Mitigation: use atomic operations or keep a short-lived mapping in the connection/session context and do strict matching during verification.

Conclusion: This registration flow is feasible given HTTPS and correct certificate validation, but it requires single-use challenges, binding of challenge and response to a protected channel (connection or session binding), and careful handling of client private key generation and storage. For stronger guarantees consider mTLS or server-side additional bindings (channel binding / HMAC over TLS exported key).
