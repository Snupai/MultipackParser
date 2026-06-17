# Signed Update Setup

This project now expects signed update metadata for online/offline updates.

## Files produced per release
- `multipack-parser-arm64.tar.gz`
- `multipack-parser-arm64-portable.run`
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

## 3) Trusted public key

The generated public key is embedded in the MultipackParser binary as the default
trusted update key.

Runtime override options:
- `MULTIPACK_UPDATE_PUBLIC_KEY_PEM` (inline PEM)
- `MULTIPACK_UPDATE_PUBLIC_KEY_FILE` (path to PEM)
- `update-public-key.pem` next to `multipack-parser`

Use an override only for development, testing, or key rotation. The private key
must never be committed or deployed.

## 4) USB/offline update layout

Put these files in the USB root directory (or `updates/` subdirectory):
- `multipack-parser-arm64.tar.gz`
- `multipack-parser-arm64-manifest.json`
- `multipack-parser-arm64-manifest.sig`

The manifest `package.file` must match the package filename.

## 5) v2 prerelease tags

GitHub Actions marks tags containing `-alpha`, `-beta`, or `-rc` as
prereleases. For the beta release, tag the prepared commit as `v2.0.0-beta`.
On tag builds, the workflow passes the tag name into CMake so the application
version embedded in the binary includes the prerelease suffix (`2.0.0-beta`),
while CMake's numeric project/package version is derived as `2.0.0`.
