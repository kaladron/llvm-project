# Step 1.4: Identifiers and Keywords

With numbers working from [Step 1.3](03-numeric-literals.md), our scanner is ready to read words.  In programming languages, word-like tokens fall into two categories:
1.  **Reserved keywords**: Words owned by the language grammar itself, such as `def` (to define a function) and `extern` (to declare an external library function like `sin` or `cos`).
2.  **Identifiers**: Names chosen by the programmer, such as variable names (`x`, `my_val`) or function names (`square`, `fib`).

---

## Recognising Words (Including Underscores)

In Kaleidoscope, a word starts with an ASCII letter or an underscore (`_`), followed by zero or more ASCII letters, digits, or underscores:

```text
Identifier / Keyword rule:  [a-zA-Z_][a-zA-Z0-9_]*
```

Supporting underscores (`_`) right from the start lets programmers write natural snake_case names like `step_size` or `my_x`, and also matches compiler-generated names like `__anon_expr` that we will meet in Section 4.  Notice why a word cannot *start* with a digit: `2fast` begins with `'2'`, which would collide with numeric scanning from [Step 1.3](03-numeric-literals.md), whereas `fast2` begins with `'f'` and scans cleanly as a single identifier!

Whenever `gettok()` sees a letter or `'_'`, it accumulates the full word into a `static std::string IdentifierStr` and then checks whether the word is a reserved keyword:
* `"def"` returns `tok_def` (`-100`).
* `"extern"` returns `tok_extern` (`-101`).
* Any other word returns `tok_identifier` (`-200`), leaving the name itself in `IdentifierStr` for the parser to read.

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and build on your [Step 1.3](03-numeric-literals.md) code.  We will walk through `def` together and let you **stretch** on `extern`!

### 1. Populate the Reserved `Token` Ranges and `IdentifierStr`

In [Step 1.3](03-numeric-literals.md), we placed `tok_number` at `-201` so keywords could live in the `-100`s and primary atoms in the `-200`s.  Update `enum Token` to add `tok_def`, `tok_extern`, and `tok_identifier`, and declare `IdentifierStr` right above `NumVal`:

```cpp
enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -100,
  tok_extern = -101,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number
```

### 2. Scan Identifiers and Keywords in `gettok()` (With a Quick Stretch!)

Inside `gettok()`, right after `while (isspace(LastChar))` and *before* the numeric literal `if`, add the identifier scanner.  Here is the logic with `def` wired in:

```cpp
  // Recognise identifiers and keywords: [a-zA-Z_][a-zA-Z0-9_]*
  if (isalpha(LastChar) || LastChar == '_') {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())) || LastChar == '_')
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    // Your stretch: recognise "extern" and return tok_extern here!
    return tok_identifier;
  }
```

**Your Stretch**: Replace that comment with the two-line check that returns `tok_extern` whenever `IdentifierStr == "extern"`.

Trace how cleanly this mirrors our number loop:
* Once `isalnum((LastChar = getchar())) || LastChar == '_'` becomes false (say on `'('` in `fib(x)`), the `while` loop stops with `LastChar` already holding `'('` for the next call to `gettok()`.
* Checking `"def"` and `"extern"` *after* reading the whole word ensures that an identifier like `default_value` is read in full as `"default_value"` (`tok_identifier`) rather than prematurely chopped into `def` + `ault_value`!

### 3. Switch on All Token Kinds in `main()`

In `main()`, update the step banner to `Step 1.4: Identifiers & Keywords` and replace the `if (tok == tok_number)` check with a `switch` statement.  Here is the skeleton with `tok_def` shown; fill in the parallel `case tok_extern:` branch (`printf("Token: KEYWORD(extern)\n");`) as part of your stretch:

```cpp
int main() {
  printf("Kaleidoscope Token Inspector (Step 1.4: Identifiers & Keywords)\n");
  printf("Type code and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != tok_eof) {
    switch (tok) {
    case tok_def:
      printf("Token: KEYWORD(def)\n");
      break;
    case tok_extern:
      // Your stretch: print "Token: KEYWORD(extern)\n" and break!
    case tok_identifier:
      printf("Token: IDENTIFIER(\"%s\")\n", IdentifierStr.c_str());
      break;
    case tok_number:
      printf("Token: NUMBER(%f)\n", NumVal);
      break;
    default:
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
      break;
    }
  }

  printf("[EOF]\n");
  return 0;
}
```

---

## Verifying Your Code

Once you have filled in both `extern` spots, open the fold below to compare your `kaleidoscope.cpp` line-for-line against the reference implementation:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -100,
  tok_extern = -101,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise identifiers and keywords: [a-zA-Z_][a-zA-Z0-9_]*
  if (isalpha(LastChar) || LastChar == '_') {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())) || LastChar == '_')
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

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
  printf("Kaleidoscope Token Inspector (Step 1.4: Identifiers & Keywords)\n");
  printf("Type code and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != tok_eof) {
    switch (tok) {
    case tok_def:
      printf("Token: KEYWORD(def)\n");
      break;
    case tok_extern:
      printf("Token: KEYWORD(extern)\n");
      break;
    case tok_identifier:
      printf("Token: IDENTIFIER(\"%s\")\n", IdentifierStr.c_str());
      break;
    case tok_number:
      printf("Token: NUMBER(%f)\n", NumVal);
      break;
    default:
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
      break;
    }
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

Run the binary and test `extern`, `def`, and an identifier containing an underscore:

```bash
./kaleidoscope
```

```text
Kaleidoscope Token Inspector (Step 1.4: Identifiers & Keywords)
Type code and press Enter (Ctrl+D to exit):

extern sin(my_x); def square(x) x * x;
Token: KEYWORD(extern)
Token: IDENTIFIER("sin")
Token: '(' (ASCII 40)
Token: IDENTIFIER("my_x")
Token: ')' (ASCII 41)
Token: ';' (ASCII 59)
Token: KEYWORD(def)
Token: IDENTIFIER("square")
Token: '(' (ASCII 40)
Token: IDENTIFIER("x")
Token: ')' (ASCII 41)
Token: IDENTIFIER("x")
Token: '*' (ASCII 42)
Token: IDENTIFIER("x")
Token: ';' (ASCII 59)
[EOF]
```

Both `extern` and `def` are recognised as language keywords, while `sin`, `my_x`, `square`, and `x` come through as identifiers with their exact names intact.

In [Step 1.5](05-skipping-comments.md), we will finish our lexer by discarding `#` comments and put your scanner knowledge to the test with a hands-on challenge.

---

[Previous: Step 1.3 - Numeric Literals](03-numeric-literals.md) | [Section 1 Overview](index.md) | [Next: Step 1.5 - Skipping Comments](05-skipping-comments.md)
