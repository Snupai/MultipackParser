# qrcodegen (vendored)

C++ QR Code generator library by Project Nayuki, MIT License (see `LICENSE`).

- Upstream: https://github.com/nayuki/QR-Code-generator
- Version: v1.8.0 (`cpp/qrcodegen.cpp`, `cpp/qrcodegen.hpp`), unmodified
- Used by `multipack::ui::QrCodeWidget` for the planner QR code on the HMI.

Vendored instead of a system package so the Raspberry Pi image needs no
extra runtime dependency.
