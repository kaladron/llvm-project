# Step 5.3: Executing Expressions (The First JIT Win)

In this step, we compile and execute top-level mathematical expressions natively in memory.

---

## The JIT Execution Pipeline

When the user types an expression like `4 + 5;`, the compiler executes the following pipeline:

1.  **Parse and Codegen**: The expression is parsed into an AST and emitted into an anonymous function named `__anon_expr` that takes no arguments and returns a `double`.
2.  **Add to JIT**: The module containing `__anon_expr` is added to `TheJIT` via `addModule()`.  This compiles the function to native machine instructions.
3.  **Symbol Lookup**: We ask `TheJIT` for the address of `__anon_expr`.
4.  **Native Execution**: Because the generated machine code conforms to the native platform C calling convention, we can cast the raw symbol address to a native C function pointer (`double (*)()`) and call it directly.
5.  **Cleanup**: A `ResourceTracker` removes the temporary expression module from memory, and `InitializeModuleAndManagers()` opens a fresh module for the next command.

---

## The Code Changes

Update `HandleTopLevelExpression()` in `kaleidoscope.cpp`:

```cpp
static void HandleTopLevelExpression() {
  if (auto FnAST = ParseTopLevelExpr()) {
    if (FnAST->codegen()) {
      // Create a ResourceTracker to free JIT memory allocated for the expression
      auto RT = TheJIT->getMainJITDylib().createResourceTracker();

      auto TSM = llvm::orc::ThreadSafeModule(std::move(TheModule),
                                             std::move(TheContext));
      ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
      InitializeModuleAndManagers();

      // Look up the symbol __anon_expr
      auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

      // Cast the symbol address to a native function pointer and call it
      double (*FP)() = ExprSymbol.toPtr<double (*)()>();
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Free the temporary expression memory
      ExitOnErr(RT->remove());
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

Type mathematical expressions:

```text
Kaleidoscope JIT REPL (Step 5.3)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 4 + 5;
Evaluated to 9.000000
ready> (1 + 2) * (10 - 4);
Evaluated to 18.000000
ready> 100 / 4 + 15;
Evaluated to 40.000000
```

Notice what just happened: your CPU executed native machine instructions produced directly by your own compiler in memory.

In [Step 5.4](04-functions-and-externs.md), we will add support for calling user-defined functions and C library functions across modules in the JIT.
