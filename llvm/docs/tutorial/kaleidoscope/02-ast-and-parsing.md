# Chapter 2: The Abstract Syntax Tree and Interactive Tree Pretty-Printing

Welcome to Chapter 2 of the Kaleidoscope tutorial.  In [Chapter 1](01-lexer.md), you built an interactive lexer that converts raw text into structured tokens.  Now we need to assemble those tokens into an Abstract Syntax Tree (AST).

An AST is an in-memory tree representation of your code.  Later stages of our compiler - including code generation and optimisation - will traverse this tree to understand what your code means.

In the original Kaleidoscope tutorial, this chapter asked you to assemble several hundred lines of parsing code, only to run it and see `Parsed a top-level expr`.  That gives you no idea whether your tree actually represents your code correctly.

In this chapter, we will build an interactive **AST Pretty-Printer**.  You will build the parser in three micro-stages.  Within ten minutes, you will type an expression and see your binary print a visual tree representation directly to your terminal.

---

## What an AST Node Looks Like

Each language construct in Kaleidoscope corresponds to a C++ class that inherits from a common base class, `ExprAST`.

For example, a numeric literal like `42.0` is represented by an instance of `NumberExprAST`, which stores the number `42.0`.  A variable reference like `x` is represented by `VariableExprAST`, which stores the identifier name `"x"`.

To give us immediate visual feedback, we will give `ExprAST` a virtual `print()` method:

```cpp
/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS) const = 0;
};
```

Whenever we parse an expression, we will call `print()` to display the tree using S-expression format (like `(Number 42)`).

---

## Stage 1: Parsing Numbers and Printing Your First Tree

Let us start with the smallest possible working parser: a minimal binary that parses numbers and prints their AST nodes.

Create a new file named `parser.cpp` in your favourite editor.  Copy in the lexer implementation from Chapter 1 (or type it by hand), and add the following AST and parser code:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// (Insert the Token enum, IdentifierStr, NumVal, and gettok() from Chapter 1 here)

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (AST)
//===----------------------------------------------------------------------===//

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

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

/// LogError - Helper function for error handling.
static std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

/// numberexpr ::= number
static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number token
  return Result;
}

/// expression ::= numberexpr
static std::unique_ptr<ExprAST> ParseExpression() {
  if (CurTok == tok_number)
    return ParseNumberExpr();
  return LogError("expected a number");
}

//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      getNextToken(); // ignore top-level semicolons
      break;
    default:
      if (auto AST = ParseExpression()) {
        std::cout << "Parsed AST: ";
        AST->print(std::cout);
        std::cout << "\n";
      } else {
        getNextToken(); // advance token to recover from error
      }
      break;
    }
  }
}

int main() {
  printf("Kaleidoscope AST Inspector (Stage 1)\n");
  printf("Type a number followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  getNextToken(); // prime the first token
  MainLoop();

  return 0;
}
```

### Compiling and Running Stage 1

Compile the executable using Clang with C++17:

```bash
clang++ -std=c++17 -Wall -Wextra parser.cpp -o parser
```

Run your new parser:

```bash
./parser
```

Type a few numbers followed by semicolons:

```text
Kaleidoscope AST Inspector (Stage 1)
Type a number followed by ';' and press Enter (Ctrl+D to exit):

ready> 42;
Parsed AST: (Number 42)
ready> 3.14159;
Parsed AST: (Number 3.14159)
ready> foo;
Error: expected a number
ready>
```

You have a functioning recursive-descent parser and an AST tree printer running in your terminal.

---

## Stage 2: Parentheses and Grouping Expressions

Arithmetic expressions frequently use parentheses to override precedence or group terms, such as `(42)`.

To parse parenthesised expressions, we introduce a new function: `ParseParenExpr()`.  Notice that this function does not create a new AST node.  Parentheses alter how expressions are grouped during parsing, but once the inner expression tree is built, the parentheses themselves carry no runtime semantic meaning.

Add `ParseParenExpr()` to `parser.cpp`:

```cpp
static std::unique_ptr<ExprAST> ParseExpression();

/// parenexpr ::= '(' expression ')'
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

Next, introduce a new dispatch function called `ParsePrimary()`.  In compiler terminology, a *primary expression* is an indivisible leaf unit of an expression - a single number, a variable reference, or a parenthesised subexpression.

```cpp
/// primary
///   ::= numberexpr
///   ::= parenexpr
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

/// expression ::= primary
static std::unique_ptr<ExprAST> ParseExpression() {
  return ParsePrimary();
}
```

### Compiling and Running Stage 2

Recompile and run:

```bash
clang++ -std=c++17 -Wall -Wextra parser.cpp -o parser
./parser
```

Try nesting multiple layers of parentheses, and test missing brackets:

```text
ready> (42);
Parsed AST: (Number 42)
ready> (((123.45)));
Parsed AST: (Number 123.45)
ready> (42;
Error: expected ')'
```

The recursive-descent parser correctly unwinds nested grouping and reports syntax errors when closing parentheses are omitted.

---

## Stage 3: Variable References and Function Calls

The final primary expressions we need are variable references (like `x`) and function calls (like `sqrt(16)` or `add(a, b)`).

First, define the AST classes for variables and calls:

```cpp
/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}

  void print(std::ostream &OS) const override {
    OS << "(Var " << Name << ")";
  }
};

/// CallExprAST - Expression class for function calls, like "foo(a, 2.0)".
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(std::string Callee, std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(std::move(Callee)), Args(std::move(Args)) {}

  void print(std::ostream &OS) const override {
    OS << "(Call " << Callee;
    for (const auto &Arg : Args) {
      OS << " ";
      Arg->print(OS);
    }
    OS << ")";
  }
};
```

Notice that `CallExprAST` stores a list of arguments as `std::vector<std::unique_ptr<ExprAST>>`.  This allows each argument to be any valid expression.

Now write `ParseIdentifierExpr()`.  When the parser encounters an identifier token, it checks whether the subsequent token is `(`.  If so, it parses comma-separated arguments for a function call.  If not, it treats the identifier as a variable reference:

```cpp
/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  // Simple variable reference:
  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(IdName);

  // Function call:
  getNextToken(); // eat '('
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("expected ')' or ',' in argument list");
      getNextToken(); // eat ','
    }
  }

  getNextToken(); // eat ')'
  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}
```

Finally, hook `ParseIdentifierExpr()` into `ParsePrimary()`:

```cpp
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}
```

### Compiling and Running Stage 3

Recompile and run:

```bash
clang++ -std=c++17 -Wall -Wextra parser.cpp -o parser
./parser
```

Test single variables and function calls with multiple arguments:

```text
ready> x;
Parsed AST: (Var x)
ready> foo(x, 10);
Parsed AST: (Call foo (Var x) (Number 10))
ready> hypot(a, (b));
Parsed AST: (Call hypot (Var a) (Number b))
```

Your parser now handles identifiers, variables, nested parentheses, and multi-argument function calls, displaying the resulting syntax tree clearly.

---

## Active Learning Challenge: Indented AST Trees

S-expressions are compact, but deep trees can be easier to read when printed as an indented outline.

### The Challenge

Modify the `print()` method on `ExprAST` to take an optional `int Indent = 0` parameter.  Update each AST class so that children are printed on new lines indented by two spaces per level.

For example, when you type `foo(x, (42));`, your binary should print:

```text
Call: foo
  Var: x
  Number: 42
```

Give this a try by updating the `print()` methods in `ExprAST`, `NumberExprAST`, `VariableExprAST`, and `CallExprAST` before opening the fold below.

<details>
<summary>Click here to reveal the solution</summary>

Here is how you can implement indented printing:

1.  Update `ExprAST`:
```cpp
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS, int Indent = 0) const = 0;
};
```

2.  Update `NumberExprAST`:
```cpp
  void print(std::ostream &OS, int Indent = 0) const override {
    OS << std::string(Indent, ' ') << "Number: " << Val << "\n";
  }
```

3.  Update `VariableExprAST`:
```cpp
  void print(std::ostream &OS, int Indent = 0) const override {
    OS << std::string(Indent, ' ') << "Var: " << Name << "\n";
  }
```

4.  Update `CallExprAST`:
```cpp
  void print(std::ostream &OS, int Indent = 0) const override {
    OS << std::string(Indent, ' ') << "Call: " << Callee << "\n";
    for (const auto &Arg : Args)
      Arg->print(OS, Indent + 2);
  }
```
</details>

---

## Complete Reference Code

The complete reference implementation for this chapter is available at [code/chapter02/parser.cpp](code/chapter02/parser.cpp).

In [Chapter 3](03-operators-and-eval.md), we will introduce binary arithmetic operators (`+`, `-`, `*`, `/`) using operator precedence climbing, and add a direct evaluation loop in pure C++ so that mathematical expressions calculate their results immediately.
