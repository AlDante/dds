# MacPorts `libffi` bug report template

## Summary

`devel/libffi` fails to build from source on Apple Silicon with current Darwin 25 / Xcode 26 CLT because the assembler rejects AArch64 CFI directives in `src/aarch64/sysv.S`.

## Environment

- macOS: `26.3.1`
- Darwin: `25.3.0`
- Architecture: `arm64`
- MacPorts: `2.12.4`
- Xcode: `26.3`
- Command Line Tools: `26.3.0.0.1.1771626560`
- SDK: `MacOSX26.sdk`
- Port: `libffi @3.4.6_1`

## Failure

The build fails while assembling `src/aarch64/sysv.S`.

Relevant log excerpt:

```text
/opt/local/var/macports/build/libffi-.../work/.tmp/sysv-....s:28:2: error: invalid CFI advance_loc expression
 .cfi_def_cfa x1, 40;
 ^
/opt/local/var/macports/build/libffi-.../work/.tmp/sysv-....s:255:2: error: invalid CFI advance_loc expression
 .cfi_adjust_cfa_offset (8*2 + (8 * 16 + 8 * 8) + 64)
 ^
make[3]: *** [src/aarch64/sysv.lo] Error 1
```

## Notes

- No binary archive appears to be available for this platform revision, so MacPorts falls back to a local source build.
- The failure appears to be caused by Apple assembler incompatibility with the emitted CFI syntax.

## Proposed patch

I have attached a local workaround patch that disables `libffi`'s emitted CFI
pseudo-ops on Apple arm64 in `include/ffi_cfi.h`.

This is a pragmatic build-unblocker for Darwin 25 / Xcode 26, intended to get
past Apple's current assembler rejection of AArch64 `.cfi_*` directives. It
may reduce unwind/debug metadata quality for these assembly routines.

Patch file:

- `macports-libffi-darwin25-cfi.patch`

## Reproduction

```zsh
sudo port selfupdate
sudo port clean --all libffi doxygen
sudo port install libffi
```

## Suggested title

```text
libffi @3.4.6_1 fails to build on darwin 25 arm64 due to AArch64 CFI assembler errors
```

## Where to file

File the report here:

- https://trac.macports.org/newticket

Recommended fields:

- **Summary**: use the suggested title above
- **Type**: defect
- **Port**: libffi
- **Version**: 3.4.6_1
- **Platform**: macOS / arm64
- **Attachments**:
  - the full `main.log`
  - `docs/macports-libffi-darwin25-cfi.patch`

## Useful attachments/commands

Attach the failing log:

- `/opt/local/var/macports/logs/_opt_local_var_macports_sources_github.com_macports_macports-ports_archive_master_ports_devel_libffi/libffi/main.log`

If you want to include a concise environment block, these are useful commands to paste into the ticket output section:

```zsh
port version
sw_vers
uname -a
xcodebuild -version
pkgutil --pkg-info=com.apple.pkg.CLTools_Executables
```

