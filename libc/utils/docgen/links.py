# ===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ===----------------------------------------------------------------------===##
"""External specification URL resolvers (cppreference.com and POSIX.1-2024)."""

from typing import Dict, Set

from header import Header

_CPPREFERENCE_BASE = "https://en.cppreference.com/w/c"


def _expand(groups: Dict[str, str]) -> Dict[str, str]:
    return {
        sym: path.format(sym=sym)
        for path, syms in groups.items()
        for sym in syms.split()
    }


_HEADER_MAP = _expand(
    {
        "error/assert": "assert.h",
        "numeric/complex": "complex.h",
        "string/byte": "ctype.h string.h",
        "error/errno": "errno.h",
        "numeric/fenv": "fenv.h",
        "types/limits": "float.h limits.h",
        "types/integer": "inttypes.h stdint.h",
        "language/operator_alternative": "iso646.h",
        "locale": "locale.h",
        "numeric/math": "math.h",
        "program": "setjmp.h signal.h stdlib.h",
        "types": "stdalign.h stdbool.h stddef.h stdnoreturn.h",
        "variadic": "stdarg.h",
        "atomic": "stdatomic.h",
        "numeric/bit": "stdbit.h",
        "numeric/ckd_arith": "stdckdint.h",
        "io": "stdio.h",
        "numeric/tgmath": "tgmath.h",
        "thread": "threads.h",
        "chrono": "time.h",
        "string/multibyte": "uchar.h",
        "string/wide": "wchar.h wctype.h",
    }
)

_HEADER_ONLY = set(
    "assert.h iso646.h stdalign.h stdbool.h stdckdint.h "
    "stdint.h stdnoreturn.h tgmath.h".split()
)
_FALLBACK_TO_HEADER = set(
    "errno.h fenv.h float.h inttypes.h limits.h locale.h "
    "setjmp.h signal.h stdarg.h stddef.h stdlib.h".split()
)

_SYMBOL_MAP = _expand(
    {
        "types/{sym}": "size_t ptrdiff_t nullptr_t max_align_t NULL offsetof",
        "types": "wchar_t",
        "types/limits": "MB_CUR_MAX",
        "error": "errno_t rsize_t",
        "error/errno_macros": "EDOM EILSEQ ERANGE",
        "variadic/{sym}": "va_start va_arg va_copy va_end va_list",
        "program/{sym}": (
            "abort exit quick_exit _Exit atexit at_quick_exit system getenv "
            "unreachable signal raise sig_atomic_t setjmp longjmp jmp_buf"
        ),
        "program/EXIT_status": "EXIT_SUCCESS EXIT_FAILURE",
        "program/SIG_strategies": "SIG_DFL SIG_ERR SIG_IGN",
        "program/SIG_types": "SIGABRT SIGFPE SIGILL SIGINT SIGSEGV SIGTERM",
        "locale/{sym}": "setlocale localeconv lconv",
        "locale/LC_categories": (
            "LC_ALL LC_COLLATE LC_CTYPE LC_MONETARY LC_NUMERIC LC_TIME"
        ),
        "memory/{sym}": (
            "malloc calloc realloc free aligned_alloc "
            "free_sized free_aligned_sized memalignment"
        ),
        "algorithm/{sym}": "qsort bsearch",
        "numeric/random/{sym}": "rand srand RAND_MAX",
        "numeric/math/abs": "abs labs llabs imaxabs",
        "numeric/math/div": "div ldiv lldiv div_t ldiv_t lldiv_t imaxdiv imaxdiv_t",
        "numeric/math/{sym}": "INFINITY NAN",
        "numeric/math/math_errhandling": "MATH_ERRNO MATH_ERREXCEPT math_errhandling",
        "numeric/math/ilogb": "FP_ILOGB0 FP_ILOGBNAN",
        "numeric/math/FP_categories": (
            "FP_INFINITE FP_NAN FP_NORMAL FP_SUBNORMAL FP_ZERO"
        ),
        "numeric/math/float_t": "float_t double_t",
        "numeric/fenv/feexcept": (
            "feclearexcept fegetexceptflag feraiseexcept "
            "fesetexceptflag fetestexcept fesetexcept fetestexceptflag"
        ),
        "numeric/fenv/feround": "fegetround fesetround",
        "numeric/fenv/feenv": "fegetenv fesetenv feholdexcept feupdateenv",
        "numeric/fenv/FE_exceptions": (
            "FE_ALL_EXCEPT FE_DIVBYZERO FE_INEXACT "
            "FE_INVALID FE_OVERFLOW FE_UNDERFLOW"
        ),
        "numeric/fenv/FE_round": (
            "FE_DOWNWARD FE_TONEAREST FE_TONEARESTFROMZERO FE_TOWARDZERO FE_UPWARD"
        ),
        "numeric/fenv/FE_DFL_ENV": "FE_DFL_ENV",
        "io/{sym}": "FILE fpos_t fgetws fputws fwide getwchar putwchar ungetwc",
        "io/fprintf": "printf fprintf sprintf snprintf printf_s",
        "io/vfprintf": "vprintf vfprintf vsprintf vsnprintf",
        "io/fscanf": "scanf fscanf sscanf",
        "io/vfscanf": "vscanf vfscanf vsscanf",
        "io/fgetc": "getc fgetc",
        "io/fputc": "putc fputc",
        "io/std_streams": "stdin stdout stderr",
        "io/fseek": "SEEK_SET SEEK_CUR SEEK_END",
        "io/fwprintf": "wprintf fwprintf swprintf",
        "io/vfwprintf": "vwprintf vfwprintf vswprintf",
        "io/fwscanf": "wscanf fwscanf swscanf",
        "io/vfwscanf": "vwscanf vfwscanf vswscanf",
        "io/fgetwc": "getwc fgetwc",
        "io/fputwc": "putwc fputwc",
        "string/multibyte/{sym}": (
            "mbstate_t mblen mbtowc wctomb mbstowcs wcstombs "
            "btowc wctob mbsinit mbrlen mbrtowc wcrtomb mbsrtowcs wcsrtombs"
        ),
        "string/byte/atof": "atof",
        "string/byte/atoi": "atoi atol atoll",
        "string/byte/strtol": "strtol strtoll",
        "string/byte/strtoul": "strtoul strtoull",
        "string/byte/strtod": (
            "strtof strtod strtold strtof16 strtof128 strtod32 strtod64 strtod128"
        ),
        "string/byte/strfromf": (
            "strfromf strfromd strfroml strfromf16 strfromf128 "
            "strfromd32 strfromd64 strfromd128"
        ),
        "string/byte/strtoimax": "strtoimax strtoumax",
        "string/byte/strlen": "strnlen_s",
        "string/wide/wcstol": "wcstol wcstoll",
        "string/wide/wcstoul": "wcstoul wcstoull",
        "string/wide/wcstof": (
            "wcstof wcstod wcstold wcstof16 wcstof128 wcstod32 wcstod64 wcstod128"
        ),
        "string/wide/wcstoimax": "wcstoimax wcstoumax",
        "string/wide/wcslen": "wcsnlen_s",
        "thread/call_once": "once_flag ONCE_FLAG_INIT",
        "chrono/gmtime": "gmtime gmtime_r gmtime_s",
        "chrono/localtime": "localtime localtime_r localtime_s",
        "chrono/{sym}": "wcsftime tm timespec",
    }
)

_FP_EXACT_BASES = set(
    "bf16mul ceil creal dmul erf f16mul fmul ilogb iscanonical isgreaterequal "
    "isinf islessequal isnormal issubnormal llogb logb modf scalb".split()
)
_FP_SUFFIXES = ("_bf16", "f128", "bf16", "f16", "d128", "d64", "d32", "f", "l")


def _strip_fp_suffix(sym: str, families: Set[str]) -> str:
    known = families | _FP_EXACT_BASES
    if sym in known or sym.isupper():
        return sym
    for sfx in _FP_SUFFIXES:
        if sym.endswith(sfx) and len(sym) > len(sfx):
            if sfx == "bf16" and sym[:-3] in known:
                continue
            return sym[: -len(sfx)]
    return sym


def cppreference_url(
    header: Header, symbol_name: str = "", kind: str = "function"
) -> str:
    hname = header.name
    hdr_path = _HEADER_MAP.get(hname)
    if not hdr_path:
        return ""
    if not symbol_name or hname in _HEADER_ONLY:
        return f"{_CPPREFERENCE_BASE}/{hdr_path}"
    sym = symbol_name[7:] if symbol_name.startswith("struct ") else symbol_name
    if sym.startswith("__STDC_ENDIAN_"):
        return f"{_CPPREFERENCE_BASE}/numeric/bit/endian"
    if sym.startswith("__"):
        return f"{_CPPREFERENCE_BASE}/{hdr_path}"
    if sym in _SYMBOL_MAP:
        return f"{_CPPREFERENCE_BASE}/{_SYMBOL_MAP[sym]}"
    if (
        hname in _FALLBACK_TO_HEADER
        or " " in symbol_name
        or (kind == "macro" and hname in ("stdio.h", "wchar.h"))
        or (kind == "type" and hname in ("uchar.h", "wchar.h", "wctype.h"))
    ):
        return f"{_CPPREFERENCE_BASE}/{hdr_path}"
    if hname == "math.h":
        if sym.startswith("HUGE_VAL"):
            return f"{_CPPREFERENCE_BASE}/numeric/math/HUGE_VAL"
        if sym.startswith("FP_FAST_FMA"):
            return f"{_CPPREFERENCE_BASE}/numeric/math/FP_FAST_FMA"
        base = _strip_fp_suffix(sym, set(header.api.get("families") or ()))
        return f"{_CPPREFERENCE_BASE}/numeric/math/{base}"
    if hname == "complex.h":
        if sym.startswith("CMPLX"):
            return f"{_CPPREFERENCE_BASE}/numeric/complex/CMPLX"
        base = _strip_fp_suffix(sym, set(header.api.get("families") or ()))
        return f"{_CPPREFERENCE_BASE}/numeric/complex/{base}"
    if hname == "stdbit.h":
        for sfx in ("_ull", "_ul", "_us", "_uc", "_ui"):
            if sym.endswith(sfx):
                return f"{_CPPREFERENCE_BASE}/numeric/bit/{sym[:-len(sfx)]}"
        return f"{_CPPREFERENCE_BASE}/numeric/bit/{sym}"
    if hname == "stdatomic.h":
        if sym.startswith("atomic_compare_exchange"):
            return f"{_CPPREFERENCE_BASE}/atomic/atomic_compare_exchange"
        if sym.endswith("_explicit"):
            return f"{_CPPREFERENCE_BASE}/atomic/{sym[:-9]}"
        if sym.startswith("ATOMIC_") and sym.endswith("_LOCK_FREE"):
            return f"{_CPPREFERENCE_BASE}/atomic/ATOMIC_LOCK_FREE_consts"
        if kind == "type" and sym not in ("atomic_flag", "memory_order"):
            return f"{_CPPREFERENCE_BASE}/atomic"
        return f"{_CPPREFERENCE_BASE}/atomic/{sym}"
    return f"{_CPPREFERENCE_BASE}/{hdr_path}/{sym}"


def posix_url(header: Header, symbol_name: str, kind: str, item: Dict) -> str:
    subpath = (
        f"functions/{symbol_name}.html"
        if kind in ("function", "family") and symbol_name
        else f"basedefs/{header.name.replace('/', '_')}.html"
    )
    if "in-latest-posix" in item:
        return f"https://pubs.opengroup.org/onlinepubs/9799919799/{subpath}"
    if "removed-in-posix-2008" in item:
        return f"https://pubs.opengroup.org/onlinepubs/007904875/{subpath}"
    if "removed-in-posix-2024" in item:
        return f"https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/{subpath}"
    return ""
