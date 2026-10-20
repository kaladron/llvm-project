# Step 1.4: Identifiers and Keywords

In this step, we add the ability to recognise words.  In programming languages, words fall into two categories:
1.  **Reserved keywords**: Words defined by the language grammar itself, such as `def` (to define a function) and `extern` (to declare a library function).
2.  **Identifiers**: Names chosen by the programmer, such as variable names (`x`, `y`) or function names (`square`, `fib`).

---

## Recognising Words

In Kaleidoscope, an identifier begins with an alphabetical letter and is followed by any combination of letters and digits (`[a-zA-Z][a-zA-Z0-9]*`).

Whenever `gettok()` sees an alphabetical character, it collects characters into a global string named `IdentifierStr`.  Once the word is complete, it checks if the word matches a keyword:
- If `IdentifierStr == "def"`, it returns `tok_def`.
- If `IdentifierStr == "extern"`, it returns `tok_extern`.
- Otherwise, it returns `tok_identifier`.

---

## The Code Changes

Update `kaleidoscope.cpp` with the new token codes and the identifier scanning block:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -2,
  tok_extern = -3,

  // Primary
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise identifiers and keywords: [a-zA-Z][a-zA-Z0-9]*
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
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
