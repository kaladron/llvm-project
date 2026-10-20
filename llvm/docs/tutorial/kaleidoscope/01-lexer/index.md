# Section 1: The Lexer (From Raw Text to Tokens)

Before a compiler can parse expressions, functions, or loops, it must break raw character text into structured units called tokens.  This stage is called lexical analysis, or lexing.

In this section, you will build a complete, interactive token inspector from scratch.  Instead of reading pages of theory or copying a large file, you will build in five bite-sized steps.  Each step takes between one and three minutes to type into your `kaleidoscope.cpp` file, compiles immediately, and gives you visual feedback in your terminal.

---

## Steps in This Section

1.  [Step 1.1: Your First Token (The 90-Second Win)](01-first-token.md) - A fifteen-line character scanner that compiles and runs immediately.
2.  [Step 1.2: Skipping Whitespace](02-skipping-whitespace.md) - Teach the scanner to discard spaces and tabs with character buffering.
3.  [Step 1.3: Numeric Literals](03-numeric-literals.md) - Read multi-digit integers and floating-point decimals.
4.  [Step 1.4: Identifiers and Keywords](04-identifiers-keywords.md) - Distinguish `def` and `extern` from variable and function names.
5.  [Step 1.5: Skipping Comments](05-skipping-comments.md) - Ignore line comments starting with `#`, plus an active learning challenge.

---

## The Living File Workflow

Throughout this section, you will work in a single file called `kaleidoscope.cpp`.  At each step, you will add five to fifteen lines of code, recompile, and run the binary to verify your progress.

If you ever want to check your code against a working reference, full implementations for each step are stored in the [code/](../code/) directory.
