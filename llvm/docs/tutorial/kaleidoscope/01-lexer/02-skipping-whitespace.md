# Step 1.2: Skipping Whitespace

In programming languages, whitespace between tokens (spaces, tabs, and newlines) carries no semantic meaning.  In this step, we introduce a dedicated function named `gettok()` that silently skips whitespace before returning the next token.

---

## Character Buffering with LastChar

When reading input character by character, we often need to look at a character to decide whether a token has ended, without discarding that character for the next token.

To handle this, our lexer stores the most recently read character in a static variable named `LastChar`.  `LastChar` is initialised to a space (`' '`) so that the first call to `gettok()` immediately triggers `getchar()` to read from standard input.

---

## The Code Changes

Open `kaleidoscope.cpp` and update it by adding `<cctype>` and our `gettok()` function above `main()`:

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

---

## Compiling and Running

Recompile:

```bash
clang++ -std=c++17 -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
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
