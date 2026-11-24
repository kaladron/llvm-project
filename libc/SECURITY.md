# Reporting LLVM-libc Security Issues

LLVM-libc is currently prerelease software. Because of this, LLVM-libc does
**not** currently participate in LLVM's
[responsible disclosure process](https://llvm.org/docs/Security.html): issues
are not handled under embargo, security advisories are not published, and CVEs
will not be generated.

Bugs in LLVM-libc, including buffer overflows or other security-relevant
defects, are valuable to report publicly
[as Issues](https://github.com/llvm/llvm-project/issues) with the `libc` label.

## Security Issue Scope

This prerelease exclusion applies **only** to the `libc/` directory, except for
code shared via [Hand-in-Hand](#hand-in-hand) below. For all other parts of the
LLVM repository, please follow the top-level [`SECURITY.md`](../SECURITY.md)
and the
[LLVM Security Group](https://llvm.org/docs/Security.html#what-is-considered-a-security-issue)
documentation.

### Hand-in-Hand

Code exposed through [Hand-in-Hand](https://libc.llvm.org/hand_in_hand.html)
(`libc/shared/` and the underlying `libc/` implementations it re-exports) is
used in other parts of the LLVM codebase, such as `libc++` and `llvm`. Security
issues affecting code consumed via Hand-in-Hand **are** covered by LLVM's
responsible disclosure policy and should be reported through the
[LLVM Security Group](https://llvm.org/docs/Security.html#how-to-report-a-security-issue)
process.
