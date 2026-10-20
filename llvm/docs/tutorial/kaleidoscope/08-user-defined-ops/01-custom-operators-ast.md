# Step 8.1: Parsing Custom Operator Prototypes

User-defined operators in Kaleidoscope use a clean, functional syntax:

```kaleidoscope
# Define unary ! (not)
def unary!(v)
  if v then 0 else 1;

# Define binary > with precedence 10
def binary> 10 (LHS RHS)
  RHS < LHS;
```

A binary operator definition includes:
1.  The keyword `binary`.
2.  The operator character (such as `>`, `|`, `%`, `:`).
3.  An optional precedence integer (between 1 and 100, defaulting to 30).
4.  Exactly two argument names inside parentheses.

A unary operator definition includes:
1.  The keyword `unary`.
2.  The operator character (such as `!`, `-`, `~`).
3.  Exactly one argument name inside parentheses.

---

## 1. Updating the Lexer

Add `tok_binary` and `tok_unary` to `Token`:

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
  tok_binary = -11,
  tok_unary = -12,
};
```

Teach `gettok()` to return these tokens when reading keywords:

```cpp
    if (IdentifierStr == "binary")
      return tok_binary;
    if (IdentifierStr == "unary")
      return tok_unary;
```

---

## 2. Updating `PrototypeAST`

Extend `PrototypeAST` to record whether a function represents an operator, and what its operator precedence is:

```cpp
class PrototypeAST {
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence;

public:
  PrototypeAST(const std::string &Name, std::vector<std::string> Args,
               bool IsOperator = false, unsigned Prec = 0)
      : Name(Name), Args(std::move(Args)), IsOperator(IsOperator),
        Precedence(Prec) {}

  llvm::Function *codegen();
  const std::string &getName() const { return Name; }
  bool isUnaryOp() const { return IsOperator && Args.size() == 1; }
  bool isBinaryOp() const { return IsOperator && Args.size() == 2; }
  char getOperatorName() const { return Name[Name.size() - 1]; }
  unsigned getBinaryPrecedence() const { return Precedence; }
};
```

---

## 3. Parsing the Prototype

Add a helper `LogErrorP` for returning `nullptr` from prototype parsing:

```cpp
static std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}
```

Now update `ParsePrototype()`:

```cpp
static std::unique_ptr<PrototypeAST> ParsePrototype() {
  std::string FnName;
  unsigned Kind = 0; // 0 = identifier, 1 = unary, 2 = binary.
  unsigned BinaryPrecedence = 30;

  switch (CurTok) {
  case tok_identifier:
    FnName = IdentifierStr;
    Kind = 0;
    getNextToken();
    break;
  case tok_unary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected unary operator");
    FnName = "unary";
    FnName += (char)CurTok;
    Kind = 1;
    getNextToken();
    break;
  case tok_binary:
    getNextToken();
    if (!isascii(CurTok))
      return LogErrorP("Expected binary operator");
    FnName = "binary";
    FnName += (char)CurTok;
    Kind = 2;
    getNextToken();

    // Read optional precedence.
    if (CurTok == tok_number) {
      if (NumVal < 1 || NumVal > 100)
        return LogErrorP("Invalid precedence: must be 1..100");
      BinaryPrecedence = (unsigned)NumVal;
      getNextToken();
    }
    break;
  default:
    return LogErrorP("Expected function name in prototype");
  }

  if (CurTok != '(')
    return LogErrorP("Expected '(' in prototype");

  std::vector<std::string> ArgNames;
  while (getNextToken() == tok_identifier)
    ArgNames.push_back(IdentifierStr);
  if (CurTok != ')')
    return LogErrorP("Expected ')' in prototype");

  getNextToken(); // eat ')'

  // Check that operators have the correct number of operands.
  if (Kind && ArgNames.size() != Kind)
    return LogErrorP("Invalid number of operands for operator");

  return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames), Kind != 0,
                                        BinaryPrecedence);
}
```

When an operator like `binary>` is defined, its LLVM function name becomes `binary>`, and its precedence is preserved in the AST.

---

Next: [Step 8.2: Unary Operators](02-unary-operators.md) - Parse and emit unary expressions.
