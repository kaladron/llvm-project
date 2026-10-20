# Step 4.4: Function Calls, Externs, and Active Learning Challenge

In this step, we add support for calling functions from IR and declaring external C standard library functions with `extern`.

---

## Function Call Code Generation

When calling a function, `CallExprAST` needs to:
1.  Look up the function in `TheModule->getFunction(Callee)`.
2.  Verify that the number of arguments matches the function prototype.
3.  Recursively generate IR for each argument.
4.  Emit an LLVM `call` instruction via `Builder->CreateCall(...)`.

Add `CallExprAST::codegen()` to `kaleidoscope.cpp`:

```cpp
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(std::string Callee, std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(std::move(Callee)), Args(std::move(Args)) {}

  llvm::Value *codegen() override {
    llvm::Function *CalleeF = TheModule->getFunction(Callee);
    if (!CalleeF) {
      fprintf(stderr, "Error: unknown function referenced: %s\n",
              Callee.c_str());
      return nullptr;
    }

    if (CalleeF->arg_size() != Args.size()) {
      fprintf(stderr, "Error: incorrect # arguments passed\n");
      return nullptr;
    }

    std::vector<llvm::Value *> ArgsV;
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
      ArgsV.push_back(Args[i]->codegen());
      if (!ArgsV.back())
        return nullptr;
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp");
  }
};
```

---

## Declaring External Functions with 'extern'

External functions are just prototypes without a function body:

```cpp
/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> ParseExtern() {
  getNextToken(); // eat 'extern'
  return ParsePrototype();
}

static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Read extern:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    getNextToken();
  }
}
```

Update `MainLoop()` to branch on `tok_extern` and call `HandleExtern()`.

---

## Compiling and Running

Compile:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core) -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Declare an external C library function and call it:

```text
Kaleidoscope LLVM IR Generator (Step 4.4: Complete Codegen)
Type definitions, externs, or expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> extern sin(x);
Read extern:
declare double @sin(double)

ready> sin(1.0);
Read top-level expression:
define double @__anon_expr() {
entry:
  %calltmp = call double @sin(double 1.000000e+00)
  ret double %calltmp
}
```

Your compiler has successfully declared an external function signature and emitted a call instruction that targets it.

---

## Active Learning Challenge: Calling User-Defined Functions

Test your understanding by writing an expression in Kaleidoscope that calls another user-defined function.

### The Challenge

Define a function `add(a b) a + b;`, and then define a second function `double_add(x) add(x, x);`.

Inspect the resulting LLVM IR to verify that `double_add` emits a `call double @add(...)` instruction.

<details>
<summary>Click here to reveal the output</summary>

```text
ready> def add(a b) a + b;
Read function definition:
define double @add(double %a, double %b) {
entry:
  %addtmp = fadd double %a, %b
  ret double %addtmp
}

ready> def double_add(x) add(x, x);
Read function definition:
define double @double_add(double %x) {
entry:
  %calltmp = call double @add(double %x, double %x)
  ret double %calltmp
}
```
</details>

---

## Next Steps

You can now translate expressions, function definitions, and library calls into validated LLVM Intermediate Representation.

In **Section 5: The JIT (Executing Native Machine Code)**, we will add an On-Request Compilation (ORC) JIT engine to compile this IR directly into executable machine code in memory.
