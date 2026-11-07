# Step 2.1: The Base AST and Numbers

Welcome to Section 2!  In [Section 1](../01-lexer/index.md), our scanner learned to turn raw source text into atomic tokens.  Now we establish our Abstract Syntax Tree (AST) class hierarchy and parse our first language construct: numeric literals.

---

## What is an Abstract Syntax Tree?

A flat list of tokens tells us *which* words and numbers appeared in the file, but not how expressions nest inside one another.  An **Abstract Syntax Tree (AST)** captures that hierarchical structure in memory:

```text
+-------------------+   Lexer    +----------------------------------+   Parser   +------------------------+
| Source Text: "42" | ---------> | Token: tok_number (NumVal = 42)  | ---------> | AST: NumberExprAST(42) |
+-------------------+            +----------------------------------+            +------------------------+
```

Why is it called *Abstract*?  Because concrete surface trivia (spaces, `#` comments, semicolons, and later parentheses) has been stripped away.  Only the operations and values that matter to execution remain.

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file from [Step 1.5](../01-lexer/05-skipping-comments.md).  Make sure you reverted the optional `if`/`then`/`else` challenge tokens so `enum Token` only contains `tok_eof`, `tok_def`, `tok_extern`, `tok_identifier`, and `tok_number`.  Now apply four updates.

### 1. New Header Includes and a Lexer Banner

At the top of `kaleidoscope.cpp`, include `<iostream>` (for `std::ostream` printing) and `<memory>` (for `std::unique_ptr`), and add an LLVM-style section banner right above `enum Token` so our growing file stays easy to navigate:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//
```

### 2. `ExprAST` and `NumberExprAST`

Every expression node in Kaleidoscope inherits from a polymorphic base class named `ExprAST`.  To give ourselves immediate visual feedback in Sections 2 and 3, we equip `ExprAST` with a pure virtual `print()` method that formats nodes as S-expressions like `(Number 42)`.

Directly below `gettok()` (replacing the old token-inspector `main()`), add the AST section:

```cpp
//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (AST)
//===----------------------------------------------------------------------===//

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS) const = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  void print(std::ostream &OS) const override {
    OS << "(Number " << Val << ")";
  }
};
```

### 3. One-Token Lookahead and Recursive-Descent Parsing

Just as our lexer in [Step 1.2](../01-lexer/02-skipping-whitespace.md) kept one character of lookahead in `LastChar`, our parser keeps **one token of lookahead** in a `static int CurTok`.  Calling `getNextToken()` pulls the next token from `gettok()` into `CurTok`.

Directly below `NumberExprAST`, add the parser section:

```cpp
//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

static std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return Result;
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_number:
    return ParseNumberExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}

static std::unique_ptr<ExprAST> ParseExpression() { return ParsePrimary(); }
```

Look at two clean design habits already visible here:
* **Ownership via `std::unique_ptr<ExprAST>`**: Every parse routine returns a `std::unique_ptr`, handing exclusive ownership of the newly allocated AST subtree up to its caller (or returning `nullptr` on a syntax error via `LogError`).
* **`ParsePrimary()` from day one**: Routing `ParseExpression()` through `ParsePrimary()` means adding parentheses, variables, and function calls in Steps 2.2 through 2.4 is as simple as adding new `case` arms to `ParsePrimary()`.

### 4. The Interactive AST REPL

In Section 1, our test loop printed tokens one by one.  Now that we are parsing full expressions, the REPL uses semicolons (`;`) as top-level statement delimiters so it knows where one expression finishes and the next prompt should appear.

:::{tip}
**Remember the Trailing Semicolon (`;`)**:
Always end your interactive inputs with a semicolon (for example, `42;`).  If you omit the semicolon and press Enter, the parser assumes your expression is still continuing on the next line and waits patiently for more tokens!
:::

Below `ParseExpression()`, add the interactive driver loop and `main()`:

```cpp
//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void Prompt() { fprintf(stderr, "ready> "); }

static void MainLoop() {
  while (true) {
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      Prompt();
      getNextToken();
      break;
    default:
      if (auto AST = ParseExpression()) {
        std::cout << "Parsed AST: ";
        AST->print(std::cout);
        std::cout << "\n";
      } else {
        // Skip tokens for error recovery until the next semicolon.
        while (CurTok != ';' && CurTok != tok_eof)
          getNextToken();
      }
      break;
    }
  }
}

int main() {
  printf("Kaleidoscope AST Inspector (Step 2.1: Numbers)\n");
  printf("Type a number followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```

Trace how `main()` and `MainLoop()` cooperate:
* `main()` prints `ready> ` first and primes `CurTok` with `getNextToken()`, blocking until you type your first line.
* `ParseNumberExpr()` consumes `42` and advances `CurTok` onto `';'`.
* `MainLoop()` prints `Parsed AST: (Number 42)`, loops around, hits `case ';':`, prints the next `ready> ` prompt, and calls `getNextToken()` to wait for your next expression.
* If `ParseExpression()` fails (returning `nullptr`), the `else` recovery loop skips ahead to `';'` so `case ';':` can cleanly print a fresh `ready> ` prompt!

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete reference file:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

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

  // Recognise comments: '#' through end of line.
  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (AST)
//===----------------------------------------------------------------------===//

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS) const = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  void print(std::ostream &OS) const override {
    OS << "(Number " << Val << ")";
  }
};

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

static std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return Result;
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_number:
    return ParseNumberExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}

static std::unique_ptr<ExprAST> ParseExpression() { return ParsePrimary(); }

//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void Prompt() { fprintf(stderr, "ready> "); }

static void MainLoop() {
  while (true) {
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      Prompt();
      getNextToken();
      break;
    default:
      if (auto AST = ParseExpression()) {
        std::cout << "Parsed AST: ";
        AST->print(std::cout);
        std::cout << "\n";
      } else {
        // Skip tokens for error recovery until the next semicolon.
        while (CurTok != ';' && CurTok != tok_eof)
          getNextToken();
      }
      break;
    }
  }
}

int main() {
  printf("Kaleidoscope AST Inspector (Step 2.1: Numbers)\n");
  printf("Type a number followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```
</details>

---

## Compiling and Running

Compile your parser:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary and test valid numbers alongside an unknown identifier:

```bash
./kaleidoscope
```

```text
Kaleidoscope AST Inspector (Step 2.1: Numbers)
Type a number followed by ';' and press Enter (Ctrl+D to exit):

ready> 42;
Parsed AST: (Number 42)
ready> 3.14159;
Parsed AST: (Number 3.14159)
ready> foo;
Error: unknown token when expecting an expression
ready>
```

:::{note}
**Why Comments and Error Recovery Feel Slightly Quirky Right Now**:
If you experiment with `#` comments from [Step 1.5](../01-lexer/05-skipping-comments.md) inside this starter REPL, you will notice two small quirks:
1.  Because `gettok()` discards `# ...` and newlines without returning a token, typing a standalone `# comment` line (or pressing Enter on a blank line) will not print a new `ready> ` prompt; the lexer simply keeps waiting for your next token.
2.  If you type a syntax error *without* a semicolon (say `foo # oops` + Enter), `ParsePrimary()` prints `Error: unknown token...` and the `while (CurTok != ';' && CurTok != tok_eof)` recovery loop keeps calling `gettok()` until you eventually type a `;` (skipping any `#` comments along the way).

Once we introduce dedicated top-level handlers (`HandleDefinition`, `HandleExtern`, and `HandleTopLevelExpression`) in Section 4, single-token recovery will advance smoothly without needing to hunt for `;`.
:::

In [Step 2.2](02-parentheses.md), we will teach our recursive-descent parser to unwrap parenthesised expressions like `(42)` and `(((123.45)))`.

---

[Previous: Step 1.5 - Skipping Comments](../01-lexer/05-skipping-comments.md) | [Section 2 Overview](index.md) | [Next: Step 2.2 - Parentheses and Grouping](02-parentheses.md)
