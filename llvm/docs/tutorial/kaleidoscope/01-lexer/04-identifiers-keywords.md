# Step 1.4: Identifiers and Keywords

In this step, we add the ability to recognise words.  In programming languages, words fall into two categories:
1.  **Reserved keywords**: Words defined by the language grammar itself, such as `def` (to define a function) and `extern` (to declare a library function).
2.  **Identifiers**: Names chosen by the programmer, such as variable names (`x`, `y`) or function names (`square`, `fib`).

---

## Recognising Words

In Kaleidoscope, an identifier begins with an alphabetical letter or underscore and is followed by any combination of letters, digits, and underscores (`[a-zA-Z_][a-zA-Z0-9_]*`).

Whenever `gettok()` sees an alphabetical character or underscore, it collects characters into a global string named `IdentifierStr`.  Once the word is complete, it checks if the word matches a keyword:
* If `IdentifierStr == "def"`, it returns `tok_def`.
* If `IdentifierStr == "extern"`, it returns `tok_extern`.
* Otherwise, it returns `tok_identifier`.

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file.  You will build upon the code you wrote in Step 1.3 by making three targeted additions.

### 1. Token Codes and IdentifierStr

Update your `enum Token` to separate token categories into distinct numeric ranges:
* Commands and keywords start at `-100` (`tok_def = -100`, `tok_extern = -101`).
* Primary expressions start at `-200` (`tok_identifier = -200`, `tok_number = -201`).

Splitting these categories numerically means that whenever you add new keywords to your language later (such as `if` or `for`), you can simply increment within the keyword range without shifting or renumbering other tokens.

Declare `IdentifierStr` directly above `NumVal`:

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

### 2. Scanning Identifiers in `gettok()`

In `gettok()`, right after the `while (isspace(LastChar))` loop skips whitespace, check if `LastChar` is an alphabetical character or an underscore.  Place this block immediately before your numeric literal check:

```cpp
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
```

Notice how this works:
* The first character is already known to be alphabetical or an underscore, so we initialise `IdentifierStr` with it.
* The `while` loop calls `getchar()` and appends characters as long as they are alphanumeric (`isalnum`) or underscores.
* Once a non-identifier character is encountered, the loop finishes.  `LastChar` holds that character, ready for the next token call.
* We then test `IdentifierStr` against our known keywords.  If it matches `"def"` or `"extern"`, we return the respective keyword token.  Otherwise, it is a user identifier.

### 3. Updating `main()` to Handle All Tokens

In `main()`, replace the previous `if-else` statement with a `switch` statement that formats keywords and identifiers:

```cpp
int main() {
  printf("Kaleidoscope Token Inspector (Step 4: Identifiers & Keywords)\n");
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

---

## Verifying Your Code

Before compiling, check that your `kaleidoscope.cpp` has all additions in place.

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
  printf("Kaleidoscope Token Inspector (Step 4: Identifiers & Keywords)\n");
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
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type a function definition prototype:

```text
Kaleidoscope Token Inspector (Step 4: Identifiers & Keywords)
Type code and press Enter (Ctrl+D to exit):

def square(x) x * x;
Token: KEYWORD(def)
Token: IDENTIFIER("square")
Token: '(' (ASCII 40)
Token: IDENTIFIER("x")
Token: ')' (ASCII 41)
Token: IDENTIFIER("x")
Token: '*' (ASCII 42)
Token: IDENTIFIER("x")
Token: ';' (ASCII 59)
```

The lexer correctly identifies `def` as a language command while preserving `square` and `x` as identifiers.

In [Step 1.5](05-skipping-comments.md), we will add comment handling and test your skills with an active learning challenge.
