# Step 1.3: Numeric Literals

In Kaleidoscope, the only primitive data type is a 64-bit floating-point number.  We want inputs like `123`, `3.14159`, and `.5` to be recognised as single numeric tokens rather than separate digits and periods.

---

## Negative Token Codes

Standard ASCII characters fall in the range `0` to `255`.  To prevent token codes from colliding with valid ASCII characters, we assign negative values to tokens that carry semantic metadata:

```cpp
enum Token {
  tok_eof = -1,
  tok_number = -2,
};
```

Whenever `gettok()` encounters a number, it will parse the numeric value, store it in a global variable named `NumVal`, and return `tok_number`.

---

## The Code Changes

Open `kaleidoscope.cpp` and update it by adding `<cstdlib>` and `<string>`, the `Token` enumeration, `NumVal`, and the digit scanning block:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
  tok_eof = -1,
  tok_number = -2,
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
  printf("Kaleidoscope Token Inspector (Step 3: Numeric Literals)\n");
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

Notice how the `do-while` loop consumes all consecutive digits and decimal points into `NumStr`.  Once a non-numeric character is seen, `strtod()` converts that string into a native `double` value.

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

Type arithmetic expressions with integers and floating-point numbers:

```text
Kaleidoscope Token Inspector (Step 3: Numeric Literals)
Type math with numbers and press Enter (Ctrl+D to exit):

3.14159 + 42 * .5;
Token: NUMBER(3.141590)
Token: '+' (ASCII 43)
Token: NUMBER(42.000000)
Token: '*' (ASCII 42)
Token: NUMBER(0.500000)
Token: ';' (ASCII 59)
```

Your scanner now cleanly isolates numeric constants from punctuation and operators.

In [Step 1.4](04-identifiers-keywords.md), we will add recognition for keywords like `def` and `extern`, along with variable and function names.
