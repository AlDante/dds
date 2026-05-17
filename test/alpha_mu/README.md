# Alpha-Mu module boundary

This directory now keeps only the remaining test-local alpha-mu headers plus
compatibility shims for older includes.

## Current files

- `tests.h` — regression-bundle entry points that remain test-only
- `api.h` — compatibility shim forwarding to `include/alpha_mu/api.h`
- `bridge.h` — compatibility shim forwarding to `include/alpha_mu/bridge.h`

The supported public wrappers now live under `include/alpha_mu/`. The shims here
keep older test-area include paths working while the remaining compatibility
surface is trimmed back to test-specific entry points.
