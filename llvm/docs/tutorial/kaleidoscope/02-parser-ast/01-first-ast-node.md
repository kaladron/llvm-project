# Step 2.1: The Base AST and Numbers

Welcome to Step 2.1.  In this step, we establish our Abstract Syntax Tree (AST) foundation and parse our first language construct: numeric literals.

---

## The Base AST Class and NumberExprAST

In C++, each AST node is represented by a class that inherits from a common base class, `ExprAST`.

To give us immediate visual confirmation that our parser works, we give `ExprAST` a virtual `print()` method:

```cpp
/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS) const = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  void print(std::ostream &OS) const override {
    OS << "(Number " << Val << ")";
  }
};
```

Whenever we parse a number, we will instantiate a `NumberExprAST` node storing the numeric value.

---

## Writing the Parser

Parsing is guided by a global token buffer `CurTok`.  Whenever the parser consumes a token, it advances to the next token by calling `getNextToken()`:

```cpp
static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

static std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume number token
  return Result;
}

/// expression ::= numberexpr
static std::unique_ptr<ExprAST> ParseExpression() {
  if (CurTok == tok_number)
    return ParseNumberExpr();
  return LogError("expected a number");
}
```

Notice what `ParseNumberExpr()` does: it takes `NumVal` (populated by our lexer in Section 1), wraps it in a `NumberExprAST`, consumes the token, and returns the AST pointer.

---

## The Interactive REPL

Add a top-level loop to `kaleidoscope.cpp` to parse expressions and print their trees:

```cpp
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      getNextToken();
      break;
    default:
      if (auto AST = ParseExpression()) {
        std::cout << "Parsed AST: ";
        AST->print(std::cout);
        std::cout << "\n";
      } else {
        getNextToken();
      }
      break;
    }
  }
}

int main() {
  printf("Kaleidoscope AST Inspector (Step 2.1: Numbers)\n");
  printf("Type a number followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  getNextToken(); // prime the first token
  MainLoop();

  return 0;
}
```

---

## Compiling and Running

Compile:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type a few numbers followed by semicolons:

```text
Kaleidoscope AST Inspector (Step 2.1: Numbers)
Type a number followed by ';' and press Enter (Ctrl+D to exit):

ready> 42;
Parsed AST: (Number 42)
ready> 3.14159;
Parsed AST: (Number 3.14159)
ready> foo;
Error: expected a number
ready>
```

You now have a running recursive-descent parser and an AST tree printer in your terminal.

In [Step 2.2](02-parentheses.md), we will add support for parenthesised expressions like `(42)`.
