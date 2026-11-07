# Step 1.3: Numeric Literals

In [Step 1.2](02-skipping-whitespace.md), every token our scanner returned was a single ASCII character: `'+'` was `43`, `'*'` was `42`, and `';'` was `59`.  The token kind and its value were the exact same integer.

Numbers are different.  An expression like `3.14159` spans seven characters in the source file, yet represents a single atomic token.  Furthermore, numbers carry a payload: the parser does not merely need to know *that* it saw a number; it needs to know *which* number (`3.14159`).

In this step, you will teach `gettok()` to group digits and decimal points into a single `tok_number` token and store its floating-point value.  Every value in early Kaleidoscope is a 64-bit IEEE-754 `double`, so integers like `42` and decimals like `0.5` share one uniform representation.

---

## Negative Token Codes, Reserved Ranges, and Payload Storage

Standard unsigned ASCII characters occupy codes `0` through `255`.  To keep multi-character tokens from colliding with single-character punctuation like `'+'` (`43`) or `';'` (`59`), our lexer uses negative numbers for named token kinds:

```text
  Token Return Code                Global Payload
+------------------------+       +------------------------+
| tok_eof    = -1        |       |                        |
| (-100s: keywords)      |       |                        |
| (-200s: primary atoms) |       |                        |
| tok_number = -201      | ----> | NumVal = 3.14159       |
| [0..255]   = ASCII char|       |                        |
+------------------------+       +------------------------+
```

Why assign `tok_number = -201` instead of `-2`?  Planning our enum ranges now saves us tedious churn later as Kaleidoscope grows across sixteen sections:
* `-1` is `tok_eof` (matching `EOF` conceptually).
* The `-100`s (`-100`, `-101`, ...) are reserved for **language keywords** (`def`, `extern`, and later `if`, `for`, `var`, and custom operator keywords).
* The `-200`s (`-200`, `-201`, ...) are reserved for **primary value tokens** (`tok_identifier = -200` in the very next step, and `tok_number = -201` right now).

Because each family has plenty of headroom in its own hundred-block, adding new keywords in later chapters never forces us to renumber existing token values!

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file and apply three updates on top of [Step 1.2](02-skipping-whitespace.md).

### 1. Headers, `enum Token`, and `NumVal`

At the top of `kaleidoscope.cpp`, add `<cstdlib>` (for `strtod`) and `<string>` (for `std::string`).  Then replace the comment above `gettok()` with `enum Token` and `NumVal`:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
  tok_eof = -1,
  tok_number = -201,
};

static double NumVal; // Filled in if tok_number
```

### 2. Scanning Digits in `gettok()`

Inside `gettok()`, right after `while (isspace(LastChar))` finishes, `LastChar` holds the first character of the next token.  If that character is a digit (`isdigit(LastChar)`) or a decimal point (`'.'`), accumulate all consecutive digits and dots into a `std::string`, convert it with `strtod`, and return `tok_number`.  Also update the `EOF` return from `-1` to `tok_eof` (and tidy away the Step 1.2 starter comments so `gettok()` stays crisp):

```cpp
static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise numeric literals: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}
```

Look at why our `LastChar` design from [Step 1.2](02-skipping-whitespace.md) pays off right away:
* Suppose the input is `42+10`.
* The `do-while` loop appends `'4'` and `'2'`, then reads `'+'` into `LastChar`.
* Because `'+'` is neither a digit nor `'.'`, the loop stops immediately without consuming `'+'`!  `LastChar` already holds `'+'` waiting for the next call to `gettok()`, with no `ungetc()` needed.

### 3. Displaying Numbers in `main()`

Update `main()` to check for `tok_eof` and print `NumVal` whenever `gettok()` returns `tok_number`:

```cpp
int main() {
  printf("Kaleidoscope Token Inspector (Step 1.3: Numeric Literals)\n");
  printf("Type math with numbers and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != tok_eof) {
    if (tok == tok_number)
      printf("Token: NUMBER(%f)\n", NumVal);
    else
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
  }

  printf("[EOF]\n");
  return 0;
}
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete reference file:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
  tok_eof = -1,
  tok_number = -201,
};

static double NumVal; // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise numeric literals: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main() {
  printf("Kaleidoscope Token Inspector (Step 1.3: Numeric Literals)\n");
  printf("Type math with numbers and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != tok_eof) {
    if (tok == tok_number)
      printf("Token: NUMBER(%f)\n", NumVal);
    else
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
  }

  printf("[EOF]\n");
  return 0;
}
```
</details>

---

## Compiling and Running

Recompile your scanner:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type an arithmetic line mixing decimals, integers, leading-dot decimals, and operators:

```text
Kaleidoscope Token Inspector (Step 1.3: Numeric Literals)
Type math with numbers and press Enter (Ctrl+D to exit):

3.14159 + 42 * .5;
Token: NUMBER(3.141590)
Token: '+' (ASCII 43)
Token: NUMBER(42.000000)
Token: '*' (ASCII 42)
Token: NUMBER(0.500000)
Token: ';' (ASCII 59)
[EOF]
```

Notice how `3.14159`, `42`, and `.5` each emerge as a single `NUMBER(...)` token while `+`, `*`, and `;` pass straight through as single-character ASCII tokens.

:::{note}
**Scanner Precision on Malformed Numbers**:
Try typing `1.2.3.4` into your token inspector.  Because our loop accepts `[0-9.]+`, it greedily collects `"1.2.3.4"` into `NumStr`, and `strtod()` parses the valid prefix `1.2` while ignoring `.3.4`.  For a tutorial compiler, keeping this loop five lines long is a great trade-off; in a production compiler, you would check the end pointer returned by `strtod()` and report a lexical error if leftover characters remained in `NumStr`.
:::

In [Step 1.4](04-identifiers-keywords.md), you will teach `gettok()` to recognise variable names, function names, and Kaleidoscope's first two keywords.

---

[Previous: Step 1.2 - Skipping Whitespace](02-skipping-whitespace.md) | [Section 1 Overview](index.md) | [Next: Step 1.4 - Identifiers and Keywords](04-identifiers-keywords.md)
