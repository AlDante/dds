# DDS Documentation

This directory contains the maintained documentation entry point for the DDS codebase.

It combines:

- curated Markdown guides for architecture and implementation work,
- generated Doxygen pages for selected public and internal headers,
- links to the historical material already present in `../doc/`.

## Prerequisite

Install `doxygen` and ensure it is available on your `PATH`.

## Build

From the repository root:

```sh
make -C docs html
```

Or from inside this directory:

```sh
make html
```

The generated HTML site will be written to:

```text
build/html/index.html
```

## Clean

```sh
make -C docs clean
```

## Contents

The generated site includes:

- `mainpage.md` — landing page
- `architecture.md` — high-level DDS architecture
- `api-overview.md` — key public APIs and usage patterns
- `data-structures.md` — important internal and public data structures
- `alpha-mu.md` — alpha-mu background and DDS integration notes
- `implementation-plan.md` — staged implementation roadmap
- `action-plan.md` — concrete next-cycle execution checklist
- `legacy-docs.md` — guide to the historical documentation in `../doc/`

Selected existing Markdown documentation from the repository is also included in the Doxygen input set.

