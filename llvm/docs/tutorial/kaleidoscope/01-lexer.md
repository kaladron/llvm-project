# Chapter 1: The Lexer and Interactive Token Inspector

Welcome to Chapter 1 of the Kaleidoscope tutorial.  Before a compiler can understand expressions, loops, or functions, it needs to break raw text into meaningful chunks called tokens.  This stage is called lexical analysis, or lexing.

In this chapter, you will build a complete, interactive token inspector from scratch.  Instead of waiting until the end of the chapter to run your code, you will build in three small stages.  Each stage takes only a few minutes to type, compiles immediately, and gives you visual feedback in your terminal.

---

## What a Lexer Does

When a human reads `def square(x) x * x;`, our brain recognises distinct units: the keyword `def`, the name `square`, the opening parenthesis `(`, the parameter `x`, and so on.

A computer sees a flat sequence of ASCII characters: `'d'`, `'e'`, `'f'`, `' '`, `'s'`, `'q'`, `'u'`, `'a'`, `'r'`, `'e'`.  The job of the lexer is to consume those characters and return structured tokens:

```text
[KEYWORD: def] [IDENT: square] ['('] [IDENT: x] [')'] [IDENT: x] ['*'] [IDENT: x] [';']
```

In Kaleidoscope, any token that does not need metadata (such as `+`, `*`, `(`, or `)`) is simply represented by its ASCII character code.  Tokens that carry meaning - like numbers, identifiers, or keywords - use negative integer codes so that they never collide with valid ASCII characters.

---

## Stage 1: The Character Scanner (Your First Run)

Let us start by writing a minimal programme that reads standard input, skips whitespace, and returns characters.  This first version is roughly thirty lines of code.

Open a new file named `lexer.cpp` in your favourite editor, and type the following code by hand:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

// The lexer returns [0-255] if it is an unknown character (like '+' or ';'),
// otherwise one of these known token enumerations.
enum Token {
  tok_eof = -1,
};

/// gettok - Return the next token from standard input.
static int gettok() {
  static int LastChar = ' ';

  // Skip any whitespace.
  while (isspace(LastChar))
    LastChar = getchar();

  // Check for end of file.
  if (LastChar == EOF)
    return tok_eof;

  // Otherwise, return the character as its ASCII value.
  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main() {
  printf("Kaleidoscope Token Inspector (Stage 1)\n");
  printf("Type characters and press Enter (Ctrl+D to exit):\n\n");

  while (true) {
    int tok = gettok();
    if (tok == tok_eof) {
      printf("[EOF]\n");
      break;
    }

    printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
  }

  return 0;
}
```

### Compiling and Running Stage 1

Compile the code with Clang using C++17 and strict warnings enabled:

```bash
clang++ -std=c++17 -Wall -Wextra lexer.cpp -o lexer
```

Now run the executable:

```bash
./lexer
```

Type `+ * ( ) ;` and press Enter.  You should immediately see output similar to this:

```text
Kaleidoscope Token Inspector (Stage 1)
Type characters and press Enter (Ctrl+D to exit):

+ * ( ) ;
Token: '+' (ASCII 43)
Token: '*' (ASCII 42)
Token: '(' (ASCII 40)
Token: ')' (ASCII 41)
Token: ';' (ASCII 59)
```

Notice what just happened: your scanner skipped the spaces between the characters automatically, returned each operator as its ASCII code, and kept running.  Press `Ctrl+D` (or `Ctrl+Z` on Windows) to send EOF and exit.

You have a working interactive scanner running in less than five minutes.

---

## Stage 2: Recognising Numeric Literals

In Kaleidoscope, the only primitive data type is a 64-bit floating-point number.  We want inputs like `123`, `3.14159`, and `.5` to be recognised as single number tokens rather than disjointed sequences of digits and dots.

To support this, we make two additions to `lexer.cpp`:

1.  Add `tok_number = -2` to our `Token` enumeration.
2.  Add a global variable `NumVal` to store the parsed floating-point value.
3.  Add digit scanning to `gettok()`.

Update `lexer.cpp` by typing these additions:

```cpp
enum Token {
  tok_eof = -1,
  tok_number = -2,
};

static double NumVal; // Filled in if tok_number
```

Now, place this block in `gettok()`, immediately after the whitespace-skipping loop:

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

Notice how this works: as long as the incoming characters are digits or decimal points, `gettok()` appends them to a string.  Once a non-numeric character is encountered, `strtod()` converts the string into a native `double` and saves it in `NumVal`.

Finally, update the loop in `main()` to print the numeric value whenever `tok_number` is returned:

```cpp
    switch (tok) {
    case tok_number:
      printf("Token: NUMBER(%f)\n", NumVal);
      break;
    default:
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
      break;
    }
```

### Compiling and Running Stage 2

Recompile and run:

```bash
clang++ -std=c++17 -Wall -Wextra lexer.cpp -o lexer
./lexer
```

Try typing an arithmetic expression with decimals and integers:

```text
3.14159 + 42 * .5;
Token: NUMBER(3.141590)
Token: '+' (ASCII 43)
Token: NUMBER(42.000000)
Token: '*' (ASCII 42)
Token: NUMBER(0.500000)
Token: ';' (ASCII 59)
```

The lexer now collapses multi-digit floating-point sequences into single numeric tokens while preserving standard operator characters.

---

## Stage 3: Identifiers, Keywords, and Comments

The last requirement for our lexer is recognising function names, variable names, and keywords like `def` and `extern`.  We also need to ignore comments starting with `#`.

Add the remaining token definitions and a string buffer for identifier names:

```cpp
enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -2,
  tok_extern = -3,

  // Primary tokens
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr; // Filled in if tok_identifier
```

In `gettok()`, place the identifier logic right after the whitespace check:

```cpp
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
```

If the first character is a letter, the lexer collects subsequent alphanumeric characters.  If the accumulated string matches a reserved keyword like `def`, it returns `tok_def`.  Otherwise, it returns `tok_identifier`, leaving the actual name in `IdentifierStr`.

Next, add comment handling immediately after the numeric literal check:

```cpp
  // Recognise comments: '#' through end of line.
  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }
```

When `gettok()` sees a `#`, it skips all characters until it reaches a newline or the end of the file.  Once the comment is consumed, it recursively calls `gettok()` to return the next real token on the following line.

Update `main()` to handle the new tokens:

```cpp
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
```

### Compiling and Running Stage 3

Recompile and run:

```bash
clang++ -std=c++17 -Wall -Wextra lexer.cpp -o lexer
./lexer
```

Now test a complete function definition with a trailing comment:

```text
def square(x) x * x; # Multiply x by itself
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

The lexer accurately distinguishes keywords from variable names, captures operators, and silently discards the comment.

---

## Active Learning Challenge: Adding New Keywords

Now that you have built the core lexer, try extending it yourself without looking at the solution first.

### The Challenge

Later in this tutorial, we will add conditional expressions to Kaleidoscope using the syntax `if x < 3 then 1 else 2`.

Extend your `lexer.cpp` to recognise three new keywords:
1.  `if` (should return `tok_if`)
2.  `then` (should return `tok_then`)
3.  `else` (should return `tok_else`)

### Testing Your Changes

When you type `if x then 1 else 2;`, your executable should print:

```text
Token: KEYWORD(if)
Token: IDENTIFIER("x")
Token: KEYWORD(then)
Token: NUMBER(1.000000)
Token: KEYWORD(else)
Token: NUMBER(2.000000)
Token: ';' (ASCII 59)
```

Give it a try before expanding the fold below!

<details>
<summary>Click here to reveal the solution</summary>

Here are the required changes:

1.  Add the new token codes to `enum Token`:
```cpp
enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,
};
```

2.  Check for the strings in the identifier block of `gettok()`:
```cpp
    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "then")
      return tok_then;
    if (IdentifierStr == "else")
      return tok_else;
    return tok_identifier;
```

3.  Add the print statements to `main()`:
```cpp
    case tok_if:
      printf("Token: KEYWORD(if)\n");
      break;
    case tok_then:
      printf("Token: KEYWORD(then)\n");
      break;
    case tok_else:
      printf("Token: KEYWORD(else)\n");
      break;
```
</details>

---

## Complete Reference Code

The complete reference implementation for this chapter is available at [code/chapter01/lexer.cpp](code/chapter01/lexer.cpp).

In [Chapter 2](02-ast-and-parsing.md), we will take this token stream and begin assembling it into an Abstract Syntax Tree (AST), complete with an interactive tree pretty-printer.
