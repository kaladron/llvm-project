# Section 8: User-Defined Operators

In [Section 7](../07-loops/index.md), you introduced `for` loops and connected host functions to draw text patterns.  However, Kaleidoscope still lacks many common mathematical and logical operators: logical negation, logical OR, logical AND, greater-than, and sequencing.

Rather than hardcoding dozens of built-in operators into the lexer and parser, Kaleidoscope lets **the programmer define custom operators directly in the language**, choosing their names, arity, and binding precedence!

In this section, you will extend your parser and code generator to support custom unary and binary operators.  By the end of this section, your compiler will execute a complete Kaleidoscope application that renders the iconic **Mandelbrot set** fractal in ASCII art inside your terminal.

---

## Steps in This Section

1.  [Step 8.1: Parsing Custom Operator Prototypes](01-custom-operators-ast.md) - Recognise `binary` and `unary` keywords in prototypes and capture user precedence.
2.  [Step 8.2: Unary Operators](02-unary-operators.md) - Parse prefix unary operators and emit function calls like `unary!(v)`.
3.  [Step 8.3: User Binary Operators](03-binary-operators.md) - Dynamically update `BinopPrecedence` and emit calls to `binary<op>(LHS, RHS)`.
4.  [Step 8.4: The Mandelbrot Set](04-mandelbrot.md) - Define standard library operators in Kaleidoscope and render the Mandelbrot fractal.

---

## Compiling Your Compiler

Compile `kaleidoscope.cpp` with `-rdynamic`:

```bash
clang++ -rdynamic -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Reference code implementations for each step are stored in the [code/](../code/) directory.
