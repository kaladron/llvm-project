#!/usr/bin/env python3
# ===----------------------------------------------------------------------===##
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ===----------------------------------------------------------------------===##
"""Unit tests for docgen and header implementation status evaluation."""

from contextlib import redirect_stdout
import io
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parent))
from docgen import print_header_status_md
from header import Header, _is_stub_file
from links import cppreference_url, posix_url


class DocgenTest(unittest.TestCase):
    def test_header_stem(self):
        self.assertEqual(Header("math.h").stem, "math")
        self.assertEqual(Header("search.h").stem, "search")
        self.assertEqual(Header("arpa/inet.h").stem, "arpa/inet")

    def test_is_stub_file(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            stub = Path(tmpdir) / "stub.cpp"
            stub.write_text(
                "// TODO\n"
                "LLVM_LIBC_FUNCTION(void *, s, ()) {\n"
                "  libc_errno = ENOSYS;\n"
                "  return nullptr;\n"
                "}\n"
            )
            self.assertTrue(_is_stub_file(stub))

            real = Path(tmpdir) / "real.cpp"
            real.write_text("LLVM_LIBC_FUNCTION(int, r, (int x)) { return x + 1; }\n")
            self.assertFalse(_is_stub_file(real))

    def test_implements_fn(self):
        self.assertTrue(Header("stdio.h").implements_fn("printf"))
        self.assertTrue(Header("netinet/in.h").implements_fn("htonl"))
        self.assertFalse(Header("stdio.h").implements_fn("sqrt"))
        self.assertFalse(Header("math.h").implements_fn("printf"))

        with tempfile.TemporaryDirectory() as tmpdir:
            root = Path(tmpdir)
            src = root / "src" / "foo"
            cfg = root / "config" / "linux"
            src.mkdir(parents=True)
            cfg.mkdir(parents=True)
            (src / "stub_fn.cpp").write_text(
                "// TODO\nLLVM_LIBC_FUNCTION(int, stub_fn, ()) { return -1; }\n"
            )
            (src / "real_fn.cpp").write_text(
                "LLVM_LIBC_FUNCTION(int, real_fn, (int x)) { return x + 1; }\n"
            )
            (cfg / "entrypoints.txt").write_text(
                "libc.src.foo.stub_fn\nlibc.src.foo.real_fn\n"
            )
            hdr = Header("foo.h", libc_root=root)
            self.assertFalse(hdr.implements_fn("stub_fn"))
            self.assertTrue(hdr.implements_fn("real_fn"))

    def test_implements_macro(self):
        self.assertTrue(Header("assert.h").implements_macro("assert"))
        self.assertFalse(Header("sys/syscall.h").macro_file_exists())
        self.assertTrue(Header("sys/syscall.h").implements_macro("SYS_read"))
        self.assertTrue(Header("errno.h").implements_macro("errno"))
        self.assertTrue(Header("unistd.h").implements_macro("SEEK_SET"))
        self.assertTrue(Header("float.h").implements_macro("NAN"))

    def test_md_output_generation(self):
        hdr = Header("stdio.h")
        api = {
            "macros": {"EOF": {"c-definition": "7.23.1", "in-latest-posix": None}},
            "functions": {
                "printf": {"c-definition": "7.23.6.3", "in-latest-posix": None},
                "printf_s": {"c-definition": "K.3.5.3.3"},
                "missing_fn": {"c-definition": "7.23.6.99"},
                "gnu_fn": {"c-definition": "TODO: glibc extension"},
            },
        }
        buf = io.StringIO()
        with redirect_stdout(buf):
            print_header_status_md(hdr, api)
        out = buf.getvalue()
        lines = {line.strip() for line in out.splitlines()}
        self.assertIn("# stdio.h", out)
        self.assertIn("## Macros", out)
        self.assertIn("## Functions", out)
        self.assertIn('<table class="hc-table">', out)
        self.assertTrue(
            any(
                'id="macro-EOF"' in l
                and 'title="Implemented"' in l
                and 'href="https://en.cppreference.com/w/c/io"' in l
                and "basedefs/stdio.h.html" in l
                and '<span aria-hidden="true">&#8599;</span>' in l
                for l in lines
            )
        )
        self.assertTrue(
            any(
                'id="function-printf"' in l
                and 'title="Implemented"' in l
                and 'href="https://en.cppreference.com/w/c/io/fprintf"' in l
                and "functions/printf.html" in l
                for l in lines
            )
        )
        self.assertTrue(
            any(
                'id="function-printf_s"' in l
                and 'href="https://en.cppreference.com/w/c/io/fprintf"' in l
                for l in lines
            )
        )
        self.assertTrue(
            any(
                'id="function-missing_fn"' in l and 'title="Not Implemented"' in l
                for l in lines
            )
        )
        self.assertTrue(
            any(
                'id="function-gnu_fn"' in l
                and '<span class="hc-sec-badge">TODO: glibc extension</span>' in l
                for l in lines
            )
        )

    def test_external_urls(self):
        cases = [
            (
                "stdio.h",
                "printf",
                "function",
                "https://en.cppreference.com/w/c/io/fprintf",
            ),
            (
                "stdlib.h",
                "malloc",
                "function",
                "https://en.cppreference.com/w/c/memory/malloc",
            ),
            (
                "stdlib.h",
                "strfromf128",
                "function",
                "https://en.cppreference.com/w/c/string/byte/strfromf",
            ),
            (
                "math.h",
                "acosf128",
                "function",
                "https://en.cppreference.com/w/c/numeric/math/acos",
            ),
            (
                "math.h",
                "erf",
                "function",
                "https://en.cppreference.com/w/c/numeric/math/erf",
            ),
            (
                "math.h",
                "ilogbf16",
                "function",
                "https://en.cppreference.com/w/c/numeric/math/ilogb",
            ),
            (
                "math.h",
                "ilogbbf16",
                "function",
                "https://en.cppreference.com/w/c/numeric/math/ilogb",
            ),
            (
                "math.h",
                "log_bf16",
                "function",
                "https://en.cppreference.com/w/c/numeric/math/log",
            ),
            (
                "complex.h",
                "creall",
                "function",
                "https://en.cppreference.com/w/c/numeric/complex/creal",
            ),
            (
                "stdbit.h",
                "stdc_bit_ceil_ui",
                "function",
                "https://en.cppreference.com/w/c/numeric/bit/stdc_bit_ceil",
            ),
            (
                "time.h",
                "gmtime_r",
                "function",
                "https://en.cppreference.com/w/c/chrono/gmtime",
            ),
            (
                "time.h",
                "struct tm",
                "type",
                "https://en.cppreference.com/w/c/chrono/tm",
            ),
            ("unistd.h", "fork", "function", ""),
        ]
        for hname, sym, kind, expected in cases:
            with self.subTest(header=hname, symbol=sym):
                self.assertEqual(cppreference_url(Header(hname), sym, kind), expected)
        self.assertEqual(
            posix_url(
                Header("sys/stat.h"), "chmod", "function", {"in-latest-posix": None}
            ),
            "https://pubs.opengroup.org/onlinepubs/9799919799/functions/chmod.html",
        )
        self.assertEqual(
            posix_url(
                Header("sys/stat.h"), "S_IRUSR", "macro", {"in-latest-posix": None}
            ),
            "https://pubs.opengroup.org/onlinepubs/9799919799/basedefs/sys_stat.h.html",
        )
        self.assertEqual(
            posix_url(
                Header("strings.h"), "bcmp", "function", {"removed-in-posix-2008": None}
            ),
            "https://pubs.opengroup.org/onlinepubs/007904875/functions/bcmp.html",
        )
        self.assertEqual(
            posix_url(
                Header("sys/time.h"),
                "gettimeofday",
                "function",
                {"removed-in-posix-2024": None},
            ),
            "https://pubs.opengroup.org/onlinepubs/9699919799.2018edition/functions/gettimeofday.html",
        )
        self.assertEqual(posix_url(Header("stdio.h"), "printf", "function", {}), "")


if __name__ == "__main__":
    unittest.main()
