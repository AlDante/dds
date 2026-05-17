# Legacy and Existing Documentation

DDS already ships with a substantial body of documentation under `doc/` and in the repository root.

## Primary existing documents

The most important existing text sources are:

- [`../README.md`](../README.md)
- [`../INSTALL`](../INSTALL)
- `../doc/dll-description.md`
- [`../doc/alpha-mu-integration.md`](../doc/alpha-mu-integration.md)
- [`../examples/README`](../examples/README)

These files remain authoritative historical sources.

For the generated documentation site, the legacy DLL description is now exposed
through the maintained guide [legacy-dll-description-guide.md](legacy-dll-description-guide.md)
rather than being parsed directly.

## Historical archive under `doc/`

The `doc/` directory also contains older material in PDF, RTF, HTML, and office-document formats. In particular, it preserves:

- historical DLL/API descriptions,
- algorithm notes,
- performance and benchmarking reports,
- older export-format documentation.

Those files are useful as archival references even when they are not part of the generated HTML output.

## Recommended reading order

For current work on DDS and alpha-mu, the recommended order is:

1. `mainpage.md`
2. `architecture.md`
3. `data-structures.md`
4. `api-overview.md`
5. `alpha-mu.md`
6. `alpha-mu-test-set.md`
7. `implementation-plan.md`
8. `action-plan.md`
9. `legacy-dll-description-guide.md`
10. `../doc/dll-description.md` for the raw historical API wording and revision log

## Relationship to the new docs folder

The `docs/` folder is intended to be the maintained entry point for:

- current architecture guidance,
- current implementation planning,
- generated API browsing,
- navigation to the older documentation corpus.

It does not replace the historical `doc/` folder; it organizes and contextualizes it.

