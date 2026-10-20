# Step 5.4: Functions and Library Calls in the JIT

In this step, we enable calling user-defined functions and external C standard library functions from the JIT REPL.

---

## The Cross-Module Prototype Table

When a function definition like `def square(x) x * x;` is entered, its module is immediately compiled and added to the JIT.  A new module is then opened for subsequent commands.

When a later command calls `square(5.0);`, the new module does not contain the declaration of `square`.  Without a prototype, LLVM cannot verify the call signature.

To solve this, we maintain a prototype table:

```cpp
static std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
```

Whenever code needs to reference a function, we look it up with `getFunction()`:

```cpp
llvm::Function *getFunction(std::string Name) {
  // First, check if the function is already declared in the current module:
  if (auto *F = TheModule->getFunction(Name))
    return F;

  // Otherwise, re-declare the prototype from our symbol cache:
  auto FI = FunctionProtos.find(Name);
  if (FI != FunctionProtos.end())
    return FI->second->codegen();

  return nullptr;
}
```

Whenever an expression references a previously defined function, `getFunction()` automatically emits a function declaration into the current module, and the JIT resolves the actual machine code pointer at runtime.

---

## Updating Function and Extern Handlers

In `HandleDefinition()`, compile the function and add the module to `TheJIT`:

```cpp
static void HandleDefinition() {
  if (auto FnAST = ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Compiled function definition:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      ExitOnErr(TheJIT->addModule(
          llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
      InitializeModuleAndManagers();
    }
  } else {
    getNextToken();
  }
}
```

In `HandleExtern()`, register the prototype in `FunctionProtos`:

```cpp
static void HandleExtern() {
  if (auto ProtoAST = ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Registered extern:\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    getNextToken();
  }
}
```

---

## Compiling and Running

Compile:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Define a function, call it, and call a standard C math function:

```text
Kaleidoscope JIT REPL (Step 5.4: Complete JIT)
Type math, definitions, or externs followed by ';' and press Enter (Ctrl+D to exit):

ready> def square(x) x * x;
Compiled function definition:
define double @square(double %x) {
entry:
  %multmp = fmul double %x, %x
  ret double %multmp
}

ready> square(5.0);
Evaluated to 25.000000

ready> extern sin(x);
Registered extern:
declare double @sin(double)

ready> sin(1.0);
Evaluated to 0.841471
```

---

## Active Learning Challenge: Dynamic Math in the JIT

Now test composing your own functions with external C math routines.

### The Challenge

Declare `extern cos(x);`, and then define a function that calculates the trigonometric identity:

$$\sin^2(x) + \cos^2(x)$$

Evaluate the function for several values (such as `0.5` and `3.14159`) to verify that the result is always `1.0`.

<details>
<summary>Click here to reveal the solution</summary>

```text
ready> extern sin(x);
Registered extern:
declare double @sin(double)

ready> extern cos(x);
Registered extern:
declare double @cos(double)

ready> def trig_identity(x) sin(x) * sin(x) + cos(x) * cos(x);
Compiled function definition:
define double @trig_identity(double %x) {
entry:
  ...
}

ready> trig_identity(0.5);
Evaluated to 1.000000

ready> trig_identity(3.14159);
Evaluated to 1.000000
```
</details>

---

## What You Have Accomplished

You have built a complete, interactive programming language from scratch:
- A custom, hand-written lexer with whitespace and comment handling.
- An Abstract Syntax Tree and recursive-descent parser.
- Operator precedence climbing for mathematical expressions.
- Direct AST evaluation in pure C++.
- An LLVM IR code generator in Static Single Assignment (SSA) form.
- An On-Request Compilation (ORCv2) JIT that executes user functions and C library routines natively on your CPU.
