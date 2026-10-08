# Testing and Contributions

Scale contract coverage with change risk, not just header compilation. Host,
simulated SDK, real-SDK compilation and device validation are distinct levels.

## Routine Verification

Run from the repository root:

```sh
bash tests/headers/compile.sh
bash tests/core/compile.sh
bash tests/tool/compile.sh
bash tests/skill/compile.sh
bash tests/context/compile.sh
bash tests/run/compile.sh
bash tests/json/compile.sh
bash tests/transport/compile.sh
bash tests/providers/openai/compile.sh
bash tests/session/compile.sh
bash tests/session/jsonl_compile.sh
bash tests/memory/compile.sh
bash tests/providers/memory_markdown/compile.sh
bash tests/providers/files/compile.sh
bash tests/ports/posix/file_store_compile.sh
bash tests/ports/posix/file_store_faults.sh
bash tests/build/tool/compile.sh
bash tests/build/context/compile.sh
bash tests/build/file_store/compile.sh
bash tests/ports/espidf/compile.sh
bash tests/ports/openvela/compile.sh
bash tests/ports/rtthread/compile.sh
bash tests/ports/rtthread/build.sh
```

Scripts use temporary builds, strict compilation and assertions. OpenAI/Run
HTTP is fake; POSIX tests operate real Host files. Neither is device acceptance.

## Minimum Coverage for New Capabilities

- Happy paths and each failure boundary; reusable state after failure.
- Fixed boundaries, zero-capacity trimming, maximum inputs and alignment.
- Borrowed/owned lifecycles, reentry, cancellation and deadlines.
- First sink error, malformed UTF-8/JSON, insufficient output.
- Storage short reads/partial writes, post-publication sync failure and cleanup failure.
- Independent C99/C++11 public header compilation.
- Build selection without dependency leaks to unrelated targets.

Use sanitizer/real-SDK options actually supported by each script, not an assumed
universal variable. Measure stacks, SDK heap, certificates, network errors and
power-loss recovery on target hardware.

## Submission Rules

Keep mechanisms, domain formats and SDKs separated. Define ownership, zero
semantics and failure facts before adding public fields. Split large code/docs
changes when useful; run relevant tests and update the manual. Do not document
unimplemented options as usable features.
