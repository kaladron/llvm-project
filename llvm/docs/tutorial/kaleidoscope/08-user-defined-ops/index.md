# Section 8: User-Defined Operators

```{toctree}
:hidden:

01-custom-operators-ast
02-unary-operators
03-binary-operators
04-mandelbrot
```

In [Section 7](../07-loops/index.md), you added `for` loops and drew ASCII shapes in the terminal.  At the end of [Step 7.3](../07-loops/03-terminal-graphics.md), however, you may have noticed how minimalist Kaleidoscope's built-in operator set still is: we only hardcoded `+`, `-`, `*`, `/`, and `<`, forcing us to improvise expression sequencing with `+`!

Rather than bloating our lexer and parser with dozens of hardcoded C++ cases for `!`, `>`, `=`, `&`, `|`, and `;`, Kaleidoscope takes a much more extensible route: **programmers can define brand-new unary and binary operators right inside Kaleidoscope**, choosing both the operator symbol and its binding precedence dynamically at runtime!

By the end of this section, you will write a mini standard library of logical, comparison, negation, and sequencing operators in Kaleidoscope itself, and run an interactive JIT application that renders the **Mandelbrot set** fractal in ASCII art.

---

## Steps in This Section

1.  [Step 8.1: Parsing Custom Operator Prototypes](01-custom-operators-ast.md) - Recognise `unary` and `binary` keywords in function prototypes and record custom operator precedence.
2.  [Step 8.2: Unary Operators](02-unary-operators.md) - Parse prefix unary operators in `ParseUnary()` and lower them to `@"unary<op>"` calls.
3.  [Step 8.3: User Binary Operators](03-binary-operators.md) - Register custom precedence in `BinopPrecedence` dynamically and lower non-built-in binary operators to `@"binary<op>"` calls.
4.  [Step 8.4: Rendering the Mandelbrot Set](04-mandelbrot.md) - Combine control flow, loops, host I/O, and user-defined operators to render the Mandelbrot fractal in your terminal.

---

## Compiling Your Compiler

Throughout Section 8, compile `kaleidoscope.cpp` with `-rdynamic` so `putchard` and `printd` remain visible to ORCv2:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference implementations for every step are available in [code/index.md](../code/index.md).

---

[Previous: Section 7 - Loops and Iteration](../07-loops/index.md) | [Tutorial Home](../index.md) | [Next: Step 8.1 - Parsing Custom Operator Prototypes](01-custom-operators-ast.md)
