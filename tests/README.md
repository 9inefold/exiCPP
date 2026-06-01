# Conformance testing

This folder contains tests based on the W3C conformance suite.
Using the default `.jar`s currently requires Java 11. Provided xerces version is `2.12.2`.

Python tests require:

- `lxml` (install with `pip install lxml`)
- `jpype` (install with `pip install JPype1`)

Python tests have an optional dependency of `zipfile`, which is only used for printing
line data in `JException` traces. The tests will still run if it is not installed.

Tests are roundtripped and run against exificient/openexi, if Java is supported.

## User Info

The CLI has the following less obvious options:

`-c`/`--clear`/`--clear-cache`, which can clear the entire cache, or just specific entries.

`-r`/`--restrict`/`--restrict-to`, which can ignore everything but specific encodings.

`--diagnostic-level`/`--jvm-diagnostic-level`, which have the following options:

- `quiet/off/silent/nothing` to print nothing
- `error` to only print errors
- `warn/warning` to print errors and warnings
- `info/note` to print errors, warnings, and some info
- `extra/verbose` to print everything, plus extra debugging info

And finally, some Java specific options:

- `--jvm`/`--jvm-path`, which sets the path to a specific JVM version
- `--jvm-classpath`, which lets you pass extra JVM class paths in the glob format
- `-ea`/`--jvm-assert`/`--jvm-assertions`, which enables JVM assertions

## Developer Info

The following sections document project details and structure.
It is not necessary to understand for regular usage.

### Folders

The `exiconf` folder contains the python code that runs the test suite.

### Test Outputs

The original data sources can be found in `s/**/NAME.xml`.

Outputs for each file are in folders `o/NAME/`, where `NAME` is the original filename.
For example, the file `at/at-01.xml` would create the folder `o/at-01/`.

Files under each `o/NAME/` follow the convention `MANGLED.FORMAT.*`.

`MANGLED` is the mangled signature of the options used on a specific file.

`FORMAT` has the following meanings:

- `x?` for *exificient* outputs
- `o?` for *openexi* outputs
- `i?` for *exicpp* outputs

The following format specifiers are currently also supported, but not enabled by default
as file extensions are easier to utilize:

- `?d` for *decoded* xml
- `?e` for *encoded* exi

If we encoded `at/at-01.xml` as byte-packed, all-preserving exi file using openexi,
we would get the file `o/at-01/yPcdip.o.exi`.

If we then decoded that file with exicpp, we would get the file `o/at-01/yPcdip.oi.xml`,
where the newest `FORMAT` is appended.
