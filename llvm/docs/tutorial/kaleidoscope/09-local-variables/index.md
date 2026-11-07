# Section 9: Local Variables and Mutability

```{toctree}
:hidden:

01-alloca-stack-variables
02-var-in-expressions
03-mem2reg-optimisation
```

Up through [Section 8](../08-user-defined-ops/index.md), Kaleidoscope has been purely functional: function parameters and `for` loop counters were bound once and could never be mutated.  That forced us to write stateful calculations like `mandelconverger` in [Step 8.4](../08-user-defined-ops/04-mandelbrot.md) by passing five accumulator parameters recursively!

Adding **mutable local variables** (`var x = 1 in ...` and `x = x + 1`) raises a classic compiler question: if LLVM IR requires Static Single Assignment (SSA) form where every virtual register is assigned only once, does your frontend have to compute dominance frontiers and place `phi` nodes everywhere a variable might be reassigned?

Happily, **no!**  LLVM's universal frontend recipe has two clean halves:

1.  **Frontend (`alloca` + `load` + `store`)**: Place mutable variables in stack slots allocated in the function's `entry` block with `alloca`, read them with `load`, and mutate them with `store`.
2.  **Middle-end (`mem2reg`)**: Run LLVM's `PromotePass` (`mem2reg`), which analyses those stack slots and automatically promotes them into optimal SSA virtual registers and `phi` nodes!

---

## Steps in This Section

1.  [Step 9.1: Stack Allocation with 'alloca'](01-alloca-stack-variables.md) - Migrate `NamedValues`, function parameters, and `for` loops to entry-block `llvm::AllocaInst *` stack slots.
2.  [Step 9.2: Local Variables and Assignment](02-var-in-expressions.md) - Add RTTI-free l-value dispatch (`codegenAddress()`), the `=` assignment operator, and `var ... in` declarations, then inspect the unoptimised stack IR.
3.  [Step 9.3: Automatic SSA Promotion with 'mem2reg'](03-mem2reg-optimisation.md) - Wire up LLVM's New Pass Manager with `PromotePass` (`mem2reg`) and scalar cleanup passes to watch stack traffic vanish into pure SSA registers.

---

## Compiling Your Compiler

In [Step 9.1](01-alloca-stack-variables.md) and [Step 9.2](02-var-in-expressions.md), continue compiling with `core orcjit native`:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Starting in [Step 9.3](03-mem2reg-optimisation.md), add the `passes` component so `llvm-config` links LLVM's New Pass Manager and scalar transformation passes:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference implementations for every step are available in [code/index.md](../code/index.md).

---

[Previous: Section 8 - User-Defined Operators](../08-user-defined-ops/index.md) | [Tutorial Home](../index.md) | [Next: Step 9.1 - Stack Allocation with 'alloca'](01-alloca-stack-variables.md)
