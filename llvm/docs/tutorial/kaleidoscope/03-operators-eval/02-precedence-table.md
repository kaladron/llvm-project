# Step 3.2: The Precedence Table

In this step, we configure an operator precedence table to define the binding strength of mathematical operators.

---

## Defining Precedence Values

To parse arithmetic without hardcoding separate grammar rules for every operator, we use a lookup table.  Higher numbers represent tighter binding:

```cpp
/// BinopPrecedence - Maps operator characters to numeric precedence.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[static_cast<char>(CurTok)];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}
```

Notice what `GetTokPrecedence()` returns:
- If `CurTok` is an operator in the map, it returns its positive precedence value.
- If `CurTok` is any other character (like `;`, `)`, or an identifier), it returns `-1`.  This signals to the parser that the binary expression has ended.

---

## Installing the Operators

In `main()`, install the standard binary operators before starting the REPL loop:

```cpp
int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  printf("Kaleidoscope AST Inspector (Step 3.2: Precedence Table)\n");
  printf("Type expressions and press Enter (Ctrl+D to exit):\n\n");

  getNextToken();
  MainLoop();

  return 0;
}
```

Because `*` and `/` have precedence 40 while `+` and `-` have precedence 20, multiplication and division will bind twice as tightly as addition and subtraction.

---

## Compiling and Verifying

Recompile your binary:

```bash
clang++ -std=c++17 -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

In [Step 3.3](03-precedence-climbing.md), we will write the precedence climbing algorithm that uses this table.
