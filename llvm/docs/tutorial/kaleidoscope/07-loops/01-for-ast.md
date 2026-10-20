# Step 7.1: Lexing and Parsing 'for' Loops

Like `if / then / else`, `for` loops in Kaleidoscope are expressions.  The syntax looks like this:

```kaleidoscope
for i = 1, i < 10, 1.0 in
  putchard(42)
```

The loop specifies:
1.  **Variable Name**: An identifier (`i`).
2.  **Start Value**: An expression evaluated once before the loop begins (`1`).
3.  **End Condition**: A condition expression checked on each iteration (`i < 10`).
4.  **Step Value (Optional)**: How much to increment the counter each iteration (defaults to `1.0` if omitted).
5.  **Body**: An expression evaluated during each iteration (`putchard(42)`).

---

## 1. Updating the Lexer

Add `tok_for` and `tok_in` to the `Token` enum in `kaleidoscope.cpp`:

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
  tok_for = -9,
  tok_in = -10,
};
```

Update `gettok()` to recognise the new keyword strings:

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
    if (IdentifierStr == "for")
      return tok_for;
    if (IdentifierStr == "in")
      return tok_in;
    return tok_identifier;
```

---

## 2. Defining the AST Node

Create `ForExprAST` to hold the loop components:

```cpp
class ForExprAST : public ExprAST {
  std::string VarName;
  std::unique_ptr<ExprAST> Start, End, Step, Body;

public:
  ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
             std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
             std::unique_ptr<ExprAST> Body)
      : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
        Step(std::move(Step)), Body(std::move(Body)) {}

  llvm::Value *codegen() override;
};
```

Provide a stub for `codegen()`:

```cpp
llvm::Value *ForExprAST::codegen() {
  fprintf(stderr, "Parsed a for-loop AST node!\n");
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
}
```

---

## 3. Parsing the Loop Expression

Write `ParseForExpr()`:

```cpp
static std::unique_ptr<ExprAST> ParseForExpr() {
  getNextToken(); // eat the 'for'

  if (CurTok != tok_identifier)
    return LogError("expected identifier after for");

  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  if (CurTok != '=')
    return LogError("expected '=' after for");
  getNextToken(); // eat '='

  auto Start = ParseExpression();
  if (!Start)
    return nullptr;
  if (CurTok != ',')
    return LogError("expected ',' after for start value");
  getNextToken();

  auto End = ParseExpression();
  if (!End)
    return nullptr;

  // The step value is optional.
  std::unique_ptr<ExprAST> Step;
  if (CurTok == ',') {
    getNextToken();
    Step = ParseExpression();
    if (!Step)
      return nullptr;
  }

  if (CurTok != tok_in)
    return LogError("expected 'in' after for");
  getNextToken(); // eat 'in'

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End),
                                      std::move(Step), std::move(Body));
}
```

Add a case for `tok_for` in `ParsePrimary()`:

```cpp
  case tok_for:
    return ParseForExpr();
```

---

## 4. Compile and Test

Compile your compiler:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Launch `./kaleidoscope` and enter a loop:

```
ready> for i = 1, i < 5 in i;
Parsed a for-loop AST node!
Evaluated to 0.000000
ready>
```

Your parser cleanly validates both mandatory and optional loop parameters.

---

Next: [Step 7.2: Emitting Loop Basic Blocks and PHI Nodes](02-loop-codegen.md) - Generate LLVM IR for loops in SSA form.
