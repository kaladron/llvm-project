# Step 1.3: Numeric Literals

In Step 1.2, every token our scanner returned was a single ASCII character: `'+'` was 43, `'*'` was 42.  The token type and its value were the exact same thing.

Numbers are different.  An expression like `3.14159` is made of seven characters, but represents a single atomic token.  Furthermore, numbers carry a payload: the parser does not just need to know that a token is a number; it needs to know *which* number.

In this step, you will teach your scanner to group digits and decimals into single numeric tokens and store their numeric value.

---

## Negative Token Codes and Payload Storage

Standard ASCII characters occupy character codes `0` to `255`.  To prevent custom tokens from colliding with valid ASCII punctuation, our lexer returns negative values for special tokens.

In addition, because a function in C++ can only return a single integer value directly, we store the numeric payload in a global variable named `NumVal`:

```text
Return code:  tok_number (-2)
Payload:      NumVal = 3.14159
```

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file.  You will build upon the code you wrote in Step 1.2 by making three targeted additions.

### 1. Headers, Token Enum, and NumVal

At the top of your file, include `<cstdlib>` (for `strtod`) and `<string>` (for string accumulation).  Then define your `Token` enumeration and `NumVal` above `gettok()`:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

// The lexer returns [0-255] if it is an unknown character, or one of these
// special token values for known things.
enum Token {
  tok_eof = -1,
  tok_number = -2,
};

static double NumVal; // Filled in if tok_number
```

### 2. Scanning Digits in `gettok()`

Look inside your `gettok()` function.  After the `while (isspace(LastChar))` loop finishes, `LastChar` holds the first character of the next token.

If `LastChar` is a digit or a decimal point, we have found the start of a number.  Add this scanning block right after the whitespace loop:

```cpp
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
```

Notice how this works:
* The `do-while` loop appends `LastChar` to `NumStr` and calls `getchar()` repeatedly as long as the incoming characters are digits or dots.
* Once a non-digit character appears (such as a space or an operator), the loop terminates.  Notice that `LastChar` already holds that next character, ready for the next call to `gettok()`.
* `strtod()` converts our accumulated string into a native C++ `double` and saves it in `NumVal`.
* Finally, `gettok()` returns `tok_number`.

While you are in `gettok()`, update your EOF check to use your new enum instead of a magic number:

```cpp
  if (LastChar == EOF)
    return tok_eof;
```

### 3. Displaying Numbers in `main()`

Now update the loop inside `main()` to recognise `tok_number` and print its value:

```cpp
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

---

## Verifying Your Code

Before compiling, check that your `kaleidoscope.cpp` has all three additions in place.

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

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

Type arithmetic expressions containing integers, decimals, and operators:

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

Notice how your scanner seamlessly groups multi-digit sequences into single atomic tokens, while preserving operators and punctuation as individual characters.

---

> [!NOTE]
> **Scanner Precision**:
> Try typing `1.2.3.4.5` into your token inspector.  Your scanner will greedily collect all the digits and dots into `NumStr`, and `strtod()` will parse `1.2`, ignoring the remaining dots and digits.
> 
> In our lightweight tutorial scanner, this simple logic keeps the code compact and readable.  In a production compiler, the lexer would track the decimal point count and flag invalid formats as lexical errors.

In [Step 1.4](04-identifiers-keywords.md), you will build upon your code once more to recognise variable names, function names, and language keywords.
