# Header Contract Checks

Run from the repository root:

```sh
bash tests/headers/compile.sh
CC=clang CXX=clang++ bash tests/headers/compile.sh
```

Requires Bash, ripgrep, a C99 compiler and a C++11 compiler. The script uses
`-Wall -Wextra -Werror -pedantic -fsyntax-only` and writes no build artifacts.
It checks every public/private header standalone and twice, both combined
include orders, static initializers, callback signatures and API usage.

`contracts.c` is compile-only, not an executable example: it deliberately does
not assemble a running Agent or demonstrate failure cleanup. The `.c` modules
remain skeletons. Passing these checks does not establish linkability, runtime
correctness, memory bounds, thread safety or target-platform support.
