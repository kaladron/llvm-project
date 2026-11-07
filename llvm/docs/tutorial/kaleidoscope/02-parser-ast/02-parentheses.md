# Step 2.2: Parentheses and Grouping

In [Step 2.1](01-first-ast-node.md), our parser learned to build `NumberExprAST` nodes.  In this step, we add support for parenthesised expressions such as `(42)` and `(((123.45)))`.

---

## Why Parentheses Do Not Need Their Own AST Node

Consider the expression `(42)`.  Should we define a `ParenExprAST` class to represent the parentheses?

```text
+-----------------------+   ParseParenExpr()    +----------------------------+
| Source Text: "( 42 )" | --------------------> | AST Node: NumberExprAST(42)|
|  eat '(' -> parse 42  |    discards '(' ')'   |  (no ParenExprAST needed!) |
|         -> eat ')'    |                       +----------------------------+
+-----------------------+
```

No!  Parentheses exist in source text purely to guide *how the parser groups subexpressions* (for example, forcing `(1 + 2) * 3` to build a `*` node whose left child is `(+ 1 2)`).  Once the parser has constructed the tree in memory, the parent-child pointers *already* encode that grouping unambiguously.  Discarding `'('` and `')'` during parsing keeps our AST lean so downstream evaluators and IR generators never have to unwrap redundant wrapper nodes.

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply three small edits on top of [Step 2.1](01-first-ast-node.md).

### 1. Forward-Declare `ParseExpression()` and Add `ParseParenExpr()`

Notice the mutual recursion we are creating:
* `ParseExpression()` calls `ParsePrimary()`.
* `ParsePrimary()` calls `ParseParenExpr()` when it sees `'('`.
* `ParseParenExpr()` calls `ParseExpression()` to parse whatever expression sits inside the parentheses!

To satisfy C++'s single-pass name lookup, add a forward declaration for `ParseExpression()` right above `ParseNumberExpr()`:

```cpp
static std::unique_ptr<ExprAST> ParseExpression();
```

Then, directly below `ParseNumberExpr()`, add `ParseParenExpr()`:

```cpp
static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat '('
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ')'
  return V;
}
```

Trace every line:
1.  `getNextToken()` consumes the opening `'('`.
2.  `ParseExpression()` recursively parses the inner expression (even if that inner expression starts with *another* `'('`!).  If the inner parse fails (`!V`), we propagate `nullptr` immediately.
3.  After the inner expression finishes, `CurTok` *must* be the matching `')'`.  If it is anything else (like `';'` in `(42;`), we report `"expected ')'"` via `LogError`.
4.  Otherwise, we consume `')'` and return `V` directly.

### 2. Route `'('` Through `ParsePrimary()` (Quick Stretch!)

In compiler grammar notation, a **primary expression** is an atomic expression unit:

```text
primary
  ::= numberexpr
  ::= parenexpr
```

Look at `ParsePrimary()` from [Step 2.1](01-first-ast-node.md):

```cpp
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_number:
    return ParseNumberExpr();
  // Your stretch: add case '(' here so it calls ParseParenExpr()!
  default:
    return LogError("unknown token when expecting an expression");
  }
}
```

**Your Stretch**: Add the two-line `case '(': return ParseParenExpr();` arm right below `case tok_number:`!

### 3. Update the `main()` Banner

Update the two `printf` lines in `main()` so the REPL reflects Step 2.2:

```cpp
  printf("Kaleidoscope AST Inspector (Step 2.2: Parentheses)\n");
  printf(
      "Type expressions followed by ';' and press Enter (Ctrl+D to exit):\n\n");
```

---

## Verifying Your Code

Check your `kaleidoscope.cpp` against the complete reference file:

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

static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return Result;
}

static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat '('
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ')'
  return V;
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
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
  printf("Kaleidoscope AST Inspector (Step 2.2: Parentheses)\n");
  printf(
      "Type expressions followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```
</details>

---

## Compiling and Running

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run `./kaleidoscope` and test single parentheses, deeply nested parentheses, and an unclosed `'('`:

```bash
./kaleidoscope
```

```text
Kaleidoscope AST Inspector (Step 2.2: Parentheses)
Type expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> (42);
Parsed AST: (Number 42)
ready> (((123.45)));
Parsed AST: (Number 123.45)
ready> (42;
Error: expected ')'
ready>
```

Notice how `(((123.45)))` recursively invokes `ParseParenExpr()` three times, unwraps all three pairs of parentheses, and returns `(Number 123.45)`, while `(42;` catches the missing `')'` and recovers right back to `ready> `!

In [Step 2.3](03-variables.md), you will design `VariableExprAST` yourself and teach the parser to recognise variable names like `x` and `my_variable`.

---

[Previous: Step 2.1 - The Base AST and Numbers](01-first-ast-node.md) | [Section 2 Overview](index.md) | [Next: Step 2.3 - Variables](03-variables.md)
