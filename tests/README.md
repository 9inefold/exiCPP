# Conformance testing

This folder contains tests based on the W3C conformance suite.
Using the default `.jar`s currently requires java 11. Provided xerces version is `2.12.2`.

Python tests require:

- `lxml` (install with `pip install lxml`)
- `jpype` (install with `pip install JPype1`)

Python tests have an optional dependency of `zipfile`, which is only used for printing
line data in `JException` traces. The tests will still run if it is not installed.

Folders have the following meanings:

- `s` for the original data sources
- `x*` for *exificient* outputs
- `i*` for *exicpp* outputs
- `*d` for *decoded* xml
- `*e` for *encoded* exi

Tests are roundtripped and run against exificient, if java is supported.
