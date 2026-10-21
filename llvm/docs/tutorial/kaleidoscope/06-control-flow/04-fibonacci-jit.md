# Step 6.4: Recursive Fibonacci in the JIT

Now that your compiler handles conditional control flow and merges branches with PHI nodes, Kaleidoscope is computationally complete.  You can write arbitrary recursive algorithms.

In this step, you will verify the comparison operator, compile the classic recursive Fibonacci algorithm, and execute it interactively in your native JIT.

---

## 1. Verifying the Comparison Operator

Ensure your `BinopPrecedence` map initialises the `<` operator:

```cpp
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;
```

In `BinaryExprAST::codegen()`, make sure `<` performs a floating-point comparison and casts the 1-bit boolean result (`i1`) back to a `double`:

```cpp
  case '<':
    L = Builder->CreateFCmpULT(L, R, "cmptmp");
    return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext),
                                 "booltmp");
```

---

## 2. Compile and Run Recursive Fibonacci

Compile your compiler:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Launch `./kaleidoscope` and define the Fibonacci function:

```
ready> def fib(x)
         if x < 3 then
           1
         else
           fib(x - 1) + fib(x - 2);
Parsed a function definition.
define double @fib(double %x) {
entry:
  %cmptmp = fcmp ult double %x, 3.000000e+00
  %booltmp = uitofp i1 %cmptmp to double
  %ifcond = fcmp one double %booltmp, 0.000000e+00
  br i1 %ifcond, label %then, label %else

then:                                             ; preds = %entry
  br label %ifcont

else:                                             ; preds = %entry
  %subtmp = fsub double %x, 1.000000e+00
  %calltmp = call double @fib(double %subtmp)
  %subtmp1 = fsub double %x, 2.000000e+00
  %calltmp2 = call double @fib(double %subtmp1)
  %addtmp = fadd double %calltmp, %calltmp2
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %iftmp = phi double [ 1.000000e+00, %then ], [ %addtmp, %else ]
  ret double %iftmp
}
```

Now evaluate Fibonacci numbers in the JIT:

```
ready> fib(10);
Evaluated to 55.000000

ready> fib(20);
Evaluated to 6765.000000
```

Your code was parsed into an AST, emitted as LLVM basic blocks with SSA PHI nodes, compiled down to native host machine instructions, and executed natively on your CPU in real time.

---

## Active Learning Challenge

<details>
<summary><b>Challenge: Adding Greater-Than ('&gt;')</b></summary>

Extend your compiler to support the `>` operator.

1.  Add `BinopPrecedence['>'] = 10;` in `main()`.
2.  Add a `case '>':` in `BinaryExprAST::codegen()` using LLVM's `CreateFCmpUGT` instruction.

Test it in your REPL with `10 > 5;` and `5 > 10;`.

<br>

**Solution**:
```cpp
// In BinaryExprAST::codegen():
case '>':
  L = Builder->CreateFCmpUGT(L, R, "cmptmp");
  return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext),
                               "booltmp");
```

</details>

---

Next: [Section 7: Loops and Iteration](../07-loops/README.md) - Build `for` loops in SSA form.
