# Architecture: DEK-based File Encryption (Windows x64)

This document outlines the system architecture, data flows, and API contracts for the DEK-based encrypted file system.

## Components
- Client (Windows x64):
  - Generates file DEKs, performs file encryption/decryption locally.
  - Stores encrypted files and encrypted DEKs on local disk.
  - Authenticates to remote Key Service to request DEK decryption.
- Remote Key Service:
  - Maintains an asymmetric key pair (private key kept in HSM-like secure storage).
  - Validates user credentials and returns decrypted DEK when appropriate.
  - Supports duress password that triggers private key deletion.

## Data Flow
1. Client: generate random DEK (e.g., AES-256-GCM) and local file encryption.
2. Client: encrypt DEK with server public key (RSA-OAEP or ECIES) and persist encrypted DEK.
3. To decrypt files, client authenticates, sends encrypted DEK to server, server returns decrypted DEK over HTTPS (TLS).
4. Client uses returned DEK to decrypt files in-memory and never writes plaintext DEK to disk.

## API Contract (example)
- POST /api/v1/dek/decrypt
  - Request JSON: { "userId": "...", "password": "...", "encryptedDek": "base64" }
  - Responses:
    - 200 OK: { "dek": "base64" }
    - 403 Forbidden: { "error": "duress_triggered" } // also triggers private key deletion
    - 401 Unauthorized: { "error": "invalid_credentials" }

## Storage Formats
- Encrypted file: custom container with metadata (version, algorithm, IV, auth tag) and ciphertext.
- Encrypted DEK: base64 of asymmetric-encrypted DEK blob; include metadata for key version.

## Security Notes
- Use authenticated encryption (AES-GCM or ChaCha20-Poly1305) for file encryption.
- Use RSA-OAEP or an ECIES-like scheme with established library primitives for DEK encryption.
- Ensure private key deletion mechanism on server is secure and irreversible (HSM zeroization or secure erase policy).

## Testing
- Unit tests for crypto primitives and metadata handling.
- Integration tests for API flows (normal and duress).

## Next steps
- Implement client-side API wrapper and storage helpers.
- Define key rotation and versioning strategy.
