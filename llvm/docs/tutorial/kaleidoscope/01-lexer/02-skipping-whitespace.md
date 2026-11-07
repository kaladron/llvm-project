# Step 1.2: Skipping Whitespace

In [Step 1.1](01-first-token.md), our scanner printed `ASCII 32` for every space we typed.  In programming languages, whitespace between tokens (spaces, tabs, and newlines) carries no semantic meaning.  In this step, we restructure our code by extracting token scanning into a dedicated function named `gettok()` that silently skips whitespace before returning the next token.

---

## Character Buffering with `LastChar`

When reading input character by character, a scanner almost always needs one character of lookahead: we have to inspect the character *after* a token to tell that the current token has ended, without losing that character for the next call.

To handle this cleanly, `gettok()` stores the most recently read character in a `static` variable named `LastChar`.  We initialise `LastChar` to a space (`' '`) so that the very first call to `gettok()` enters the whitespace-skipping loop and calls `getchar()` to read from standard input.

:::{note}
**Production Code vs Pedagogical Clarity**:
Notice that `LastChar` is declared `static`, retaining its value across repeated calls to `gettok()`.  In upcoming steps, we will also use file-scope `static` variables (`NumVal` and `IdentifierStr`) to pass token payloads back to the parser.

In a production compiler, you would encapsulate this state inside a `Lexer` class rather than keeping it in `static` variables:
* Function-local and global state is neither thread-safe nor re-entrant, so multiple compiler instances cannot run concurrently in the same process.
* Saving and restoring scanner state for speculative lookahead is much easier when state lives in an object.
* A production lexer also tracks source locations (file, line, and column) on every token to print caret diagnostics.

We deliberately keep state minimal here in favour of clarity.  Skipping the extra plumbing lets you focus directly on lexing, parsing, and LLVM IR generation.
:::

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file.  Let us transform the raw loop from [Step 1.1](01-first-token.md) into a structured lexer with three small updates.

### 1. Header Inclusion

At the top of `kaleidoscope.cpp`, include `<cctype>` right above `<cstdio>` so we can call `isspace()`:

```cpp
#include <cctype>
#include <cstdio>
```

### 2. The `gettok()` Function

Above `main()`, add our primary lexing function:

```cpp
// Returns [0-255] for an ASCII character, or -1 for EOF.
static int gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getchar();

  if (LastChar == EOF)
    return -1;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}
```

Trace how `LastChar` flows from one call to the next:
* `LastChar` persists across calls to `gettok()` because it is `static`.
* The `while (isspace(LastChar))` loop repeatedly calls `getchar()` until `LastChar` holds a non-whitespace character (or `EOF`).
* If `LastChar == EOF`, we return `-1` to signal the end of input.
* Otherwise, we save `LastChar` in `ThisChar`, prime `LastChar` with the *next* character from `getchar()` for future calls, and return `ThisChar`.

:::{note}
**Terminal Line Buffering and `isspace()`**:
When running `./kaleidoscope` interactively, typing spaces or characters produces no output until you press **Enter**.

This happens because terminal input operates in **canonical mode** (line-buffered mode) by default:
* The operating system kernel holds your keystrokes in a line buffer so you can edit typos with Backspace.
* Nothing is delivered to `getchar()` until you press Enter, which appends a newline (`'\n'`).

Once you press Enter, `gettok()` consumes the buffered characters in rapid succession.  Because standard `isspace()` classifies `'\n'` and `'\r'` as whitespace right alongside `' '` and `'\t'`, the `while (isspace(LastChar))` loop swallows the trailing newline automatically and blocks cleanly on `getchar()` for your next line.  That is why the manual `c != '\n' && c != '\r'` check from Step 1.1 is no longer needed!
:::

### 3. A Streamlined `main()` Loop

Replace the body of `main()` so it updates the step banner and calls `gettok()` instead of `getchar()`:

```cpp
int main() {
  printf("Kaleidoscope Token Inspector (Step 1.2: Whitespace Skipping)\n");
  printf("Type characters with spaces and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != -1)
    printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);

  printf("[EOF]\n");
  return 0;
}
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete reference file for this step:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include <cctype>
#include <cstdio>

// Returns [0-255] for an ASCII character, or -1 for EOF.
static int gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getchar();

  if (LastChar == EOF)
    return -1;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main() {
  printf("Kaleidoscope Token Inspector (Step 1.2: Whitespace Skipping)\n");
  printf("Type characters with spaces and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != -1)
    printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);

  printf("[EOF]\n");
  return 0;
}
```
</details>

---

## Compiling and Running

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type operators separated by generous runs of spaces or tabs, press Enter, and finish with `Ctrl+D`:

```text
Kaleidoscope Token Inspector (Step 1.2: Whitespace Skipping)
Type characters with spaces and press Enter (Ctrl+D to exit):

   +       *     ;   
Token: '+' (ASCII 43)
Token: '*' (ASCII 42)
Token: ';' (ASCII 59)
[EOF]
```

Every space, tab, and newline vanished.  `gettok()` swallowed all whitespace and returned only the meaningful operator tokens.

In [Step 1.3](03-numeric-literals.md), we will teach `gettok()` to group runs of digits and decimal points into numeric literal tokens.

---

[Previous: Step 1.1 - What is a Token?](01-first-token.md) | [Section 1 Overview](index.md) | [Next: Step 1.3 - Numeric Literals](03-numeric-literals.md)
