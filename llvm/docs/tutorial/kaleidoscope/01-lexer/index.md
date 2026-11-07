# Section 1: The Lexer (From Raw Text to Tokens)

```{toctree}
:hidden:

01-first-token
02-skipping-whitespace
03-numeric-literals
04-identifiers-keywords
05-skipping-comments
```

Before a compiler can parse expressions, functions, or loops, it must break raw character text into structured units called tokens.  This stage is called lexical analysis, or lexing.

In this section, you will build a complete, interactive token inspector from scratch.  Instead of reading pages of theory or copying a large file, you will work through five bite-sized steps.  Each step takes only a couple of minutes to type into your `kaleidoscope.cpp` file, compiles immediately, and gives you live feedback in your terminal.

---

## Steps in This Section

1.  [Step 1.1: What is a Token?](01-first-token.md) - Understand tokens as atomic syntax units and build a basic single-character scanner.
2.  [Step 1.2: Skipping Whitespace](02-skipping-whitespace.md) - Teach the scanner to discard spaces, tabs, and newlines with one-character lookahead.
3.  [Step 1.3: Numeric Literals](03-numeric-literals.md) - Group multi-digit integers and floating-point decimals into single tokens with reserved enum ranges.
4.  [Step 1.4: Identifiers and Keywords](04-identifiers-keywords.md) - Distinguish `def` and `extern` from variable and function names (including underscores).
5.  [Step 1.5: Skipping Comments](05-skipping-comments.md) - Ignore line comments starting with `#`, plus an active learning challenge.

---

## The Living File Workflow

Throughout this tutorial, you will work in a single living file called `kaleidoscope.cpp`.  At each step, you will add or refine a small handful of lines, recompile, and run the binary to verify your progress.

If you ever want to check your code against a working reference, collapsible snapshots appear inside every step, and standalone step implementations are also archived in the [Reference Code Archive](../code/index.md).

---

[Tutorial Overview](../index.md) | [Start: Step 1.1 - What is a Token?](01-first-token.md) | [Next Section: The Parser and AST](../02-parser-ast/index.md)
