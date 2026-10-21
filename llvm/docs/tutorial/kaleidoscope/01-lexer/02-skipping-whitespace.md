# Step 1.2: Skipping Whitespace

In programming languages, whitespace between tokens (spaces, tabs, and newlines) carries no semantic meaning.  In this step, we restructure our code by extracting token scanning into a dedicated function named `gettok()` that silently skips whitespace before returning the next token.

---

## Character Buffering with LastChar

When reading input character by character, we often need to look at a character to decide whether a token has ended, without discarding that character for the next token.

To handle this, our lexer stores the most recently read character in a static variable named `LastChar`.  `LastChar` is initialised to a space (`' '`) so that the first call to `gettok()` immediately triggers `getchar()` to read from standard input.

> [!NOTE]
> **Production Code vs Pedagogical Clarity**:
> Notice that `LastChar` is declared `static`, retaining its value across repeated calls to `gettok()`.  In upcoming steps, we will also use global variables (`NumVal` and `IdentifierStr`) to pass token payloads back to the parser.
> 
> In production-grade compiler software, you would not use global variables or function-local `static` state:
> * They are not thread-safe or re-entrant, making it impossible to run multiple compiler threads concurrently.
> * They make error recovery and backtracking difficult because state cannot be easily saved or restored.
> * A production lexer would encapsulate this state inside a `Lexer` class, accept an input stream reference, and attach source locations (file, line, and column) to every token for rich compiler diagnostics.
> 
> We deliberately forgo that engineering complexity here in favour of clarity.  Keeping the state minimal and straightforward lets you focus directly on compiler concepts and LLVM code generation without getting bogged down in C++ boilerplate.

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file.  You will transform the raw loop from Step 1.1 into a structured lexer through three targeted updates.

### 1. Header Inclusion

At the top of `kaleidoscope.cpp`, include `<cctype>` for the `isspace()` function:

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

  // Check for end of file.
  if (LastChar == EOF)
    return -1;

  // Otherwise, return the character as its ASCII value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}
```

Notice what happens:
* `LastChar` persists across calls to `gettok()` because it is `static`.
* The `while (isspace(LastChar))` loop repeatedly calls `getchar()` until it finds a non-whitespace character.
* Once a non-whitespace character is found, we copy it to `ThisChar`, read the next character into `LastChar` for future calls, and return `ThisChar`.

### 3. A Streamlined `main()` Loop

With `gettok()` handling token extraction, your `main()` function simplifies into a clean consumer loop:

```cpp
int main() {
  printf("Kaleidoscope Token Inspector (Step 2: Whitespace Skipping)\n");
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

Before compiling, check that your `kaleidoscope.cpp` has all changes in place.

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

  // Check for end of file.
  if (LastChar == EOF)
    return -1;

  // Otherwise, return the character as its ASCII value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main() {
  printf("Kaleidoscope Token Inspector (Step 2: Whitespace Skipping)\n");
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
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type operators separated by multiple spaces or tabs:

```text
Kaleidoscope Token Inspector (Step 2: Whitespace Skipping)
Type characters with spaces and press Enter (Ctrl+D to exit):

   +       *     ;   
Token: '+' (ASCII 43)
Token: '*' (ASCII 42)
Token: ';' (ASCII 59)
```

The spaces vanished completely.  `gettok()` silently swallowed all whitespace characters and returned only the operator tokens.

In [Step 1.3](03-numeric-literals.md), we will teach `gettok()` to group digits and decimals into single numeric tokens.
