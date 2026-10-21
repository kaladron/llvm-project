# Section 9: Local Variables and Mutability

In previous sections, Kaleidoscope was primarily a functional language: variables were bound once as function arguments or loop counters and could never be reassigned.  While functional languages are elegant, most mainstream languages support **mutable local variables** that can be reassigned over time.

Generating Static Single Assignment (SSA) form for mutable variables is notoriously challenging for compiler frontends: calculating dominance frontiers and inserting PHI nodes is non-trivial.

Fortunately, **LLVM does not require your frontend to build SSA form for mutable variables**.

In this section, you will learn LLVM's standard idiom for mutable variables: allocating memory on the stack using `alloca`, reading and writing with `load` and `store`, and letting LLVM's `mem2reg` pass automatically promote those stack variables into pristine SSA registers.

---

## Steps in This Section

1.  [Step 9.1: Stack Allocation with 'alloca'](01-alloca-stack-variables.md) - Create entry block `alloca` variables, read with `load`, and write with `store`.
2.  [Step 9.2: Local Variables and Assignment](02-var-in-expressions.md) - Add `var / in` syntax and the assignment operator `=`.
3.  [Step 9.3: Automatic SSA Promotion with 'mem2reg'](03-mem2reg-optimization.md) - Run LLVM's `mem2reg` pass to eliminate memory accesses.

---

## Compiling Your Compiler

Include the `passes` library component in your compile command to link LLVM's optimisation pass manager:

```bash
clang++ -rdynamic -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes) -o kaleidoscope
```

Reference code implementations for each step are stored in the [code/](../code/) directory.
