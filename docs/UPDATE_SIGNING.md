# Signed Update Setup

This project now expects signed update metadata for online/offline updates.

## Files produced per release
- `multipack-parser-arm64.tar.gz`
- `multipack-parser-arm64-manifest.json`
- `multipack-parser-arm64-manifest.sig`

## 1) Generate an Ed25519 key pair

```bash
openssl genpkey -algorithm Ed25519 -out update-signing-private.pem
openssl pkey -in update-signing-private.pem -pubout -out update-public-key.pem
```

## 2) Configure GitHub secret for release signing

Set repository secret:
- Name: `UPDATE_SIGNING_PRIVATE_KEY_B64`
- Value: base64 of `update-signing-private.pem`

```bash
base64 < update-signing-private.pem
```

## 3) Deploy trusted public key with the app

Copy `update-public-key.pem` to the same directory as `multipack-parser` on the Raspberry Pi.

Alternative runtime options:
- `MULTIPACK_UPDATE_PUBLIC_KEY_PEM` (inline PEM)
- `MULTIPACK_UPDATE_PUBLIC_KEY_FILE` (path to PEM)

## 4) USB/offline update layout

Put these files in the USB root directory (or `updates/` subdirectory):
- `multipack-parser-arm64.tar.gz`
- `multipack-parser-arm64-manifest.json`
- `multipack-parser-arm64-manifest.sig`

The manifest `package.file` must match the package filename.
