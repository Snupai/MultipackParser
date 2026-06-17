#!/usr/bin/env python3
"""Generate signed update metadata for MultipackParser releases."""

from __future__ import annotations

import argparse
import base64
import datetime as dt
import hashlib
import json
import pathlib
import subprocess
import sys
from typing import Any


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def sign_manifest(manifest_path: pathlib.Path, private_key_path: pathlib.Path) -> bytes:
    proc = subprocess.run(
        [
            "openssl",
            "pkeyutl",
            "-sign",
            "-inkey",
            str(private_key_path),
            "-rawin",
            "-in",
            str(manifest_path),
        ],
        check=False,
        capture_output=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(
            "openssl pkeyutl failed: " + proc.stderr.decode("utf-8", errors="replace").strip()
        )
    return proc.stdout


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, help="Path to update package (tar.gz)")
    parser.add_argument("--version", required=True, help="Version string (e.g. 2.0.0-beta)")
    parser.add_argument("--output-manifest", required=True, help="Output manifest JSON path")
    parser.add_argument(
        "--package-url",
        default="",
        help="Public package URL for online updates (optional)",
    )
    parser.add_argument(
        "--release-notes",
        default="",
        help="Release notes text to include in the manifest (optional)",
    )
    parser.add_argument(
        "--published-at",
        default="",
        help="RFC3339 timestamp (defaults to current UTC)",
    )
    parser.add_argument(
        "--signing-key",
        default="",
        help="PEM private key for Ed25519 signing (optional)",
    )
    parser.add_argument(
        "--output-signature",
        default="",
        help="Output signature JSON path (required when --signing-key is set)",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    package_path = pathlib.Path(args.package).resolve()
    if not package_path.exists() or not package_path.is_file():
        print(f"error: package does not exist: {package_path}", file=sys.stderr)
        return 1

    manifest_path = pathlib.Path(args.output_manifest).resolve()
    manifest_path.parent.mkdir(parents=True, exist_ok=True)

    if args.signing_key and not args.output_signature:
        print("error: --output-signature is required when --signing-key is set", file=sys.stderr)
        return 1

    published_at = args.published_at.strip()
    if not published_at:
        published_at = dt.datetime.now(dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")

    package_sha256 = sha256_file(package_path)
    package_size = package_path.stat().st_size

    manifest: dict[str, Any] = {
        "schema_version": 1,
        "application": "MultipackParser",
        "version": args.version.lstrip("v"),
        "published_at": published_at,
        "release_notes": args.release_notes,
        "package": {
            "file": package_path.name,
            "url": args.package_url,
            "size": package_size,
            "sha256": package_sha256,
        },
    }

    manifest_json = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    manifest_path.write_text(manifest_json, encoding="utf-8")

    if args.signing_key:
        signing_key_path = pathlib.Path(args.signing_key).resolve()
        if not signing_key_path.exists() or not signing_key_path.is_file():
            print(f"error: signing key does not exist: {signing_key_path}", file=sys.stderr)
            return 1

        signature_raw = sign_manifest(manifest_path, signing_key_path)
        signature_doc = {
            "algorithm": "ed25519",
            "manifest_sha256": hashlib.sha256(manifest_json.encode("utf-8")).hexdigest(),
            "signature": base64.b64encode(signature_raw).decode("ascii"),
        }

        signature_path = pathlib.Path(args.output_signature).resolve()
        signature_path.parent.mkdir(parents=True, exist_ok=True)
        signature_json = json.dumps(signature_doc, indent=2, sort_keys=True) + "\n"
        signature_path.write_text(signature_json, encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
