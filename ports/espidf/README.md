# ESP-IDF Port

This optional Port is split into [runtime/](runtime/README.md) and
[transport/](transport/README.md). It owns ESP-IDF headers and component metadata; neither
subpackage is part of the Core build.

Both subpackages now have source and public headers. They are validated with a C99 mock SDK; an
ESP-IDF hardware integration target is still required before claiming a supported SDK release.
