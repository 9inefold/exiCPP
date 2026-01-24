# Conformance testing

This folder contains tests based on the W3C conformance suite.
Using the default exificient `.jar` requires java 8.
Python tests require `xmldiff` (install with `pip install xmldiff`).

Folders have the following meanings:

- `s` for the original data sources
- `x*` for *exificient* outputs
- `i*` for *exicpp* outputs
- `*d` for *decoded* xml
- `*e` for *encoded* exi

Tests are roundtripped and run against exificient, if java is supported.
