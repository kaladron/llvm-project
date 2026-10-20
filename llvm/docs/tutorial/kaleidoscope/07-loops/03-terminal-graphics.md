# Step 7.3: Terminal Graphics and Host Calls

With `for` loops in place, Kaleidoscope can execute repeated operations.  To observe the loop in action, we need a way to output characters to the screen.

In this step, you will expose native C helper functions from your compiler's binary to the JIT using the `-rdynamic` compiler flag, and write a Kaleidoscope function that prints terminal patterns.

---

## 1. Exposing Host Functions

At the top of `kaleidoscope.cpp`, define two C-linkage helper functions:

```cpp
extern "C" double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

extern "C" double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}
```

Because Kaleidoscope operates exclusively on floating-point `double` values, `putchard` accepts a `double`, casts it to a `char`, writes it to standard error, and returns `0.0`.

---

## 2. Why `-rdynamic` is Required

When compiling standard C++ executables, linkers discard symbols from the dynamic symbol table unless they are referenced by an external shared library.

Because your JIT compiles code in memory after your executable has already started running, the dynamic linker must be able to locate `putchard` by name inside your running process.  Passing `-rdynamic` instructs the linker to preserve all global symbols in the dynamic symbol table so `KaleidoscopeJIT` can resolve them.

Compile your compiler with `-rdynamic`:

```bash
clang++ -rdynamic -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

---

## 3. Running Loops in the JIT

Launch `./kaleidoscope` and declare the external function:

```
ready> extern putchard(x);
Parsed an extern
declare double @putchard(double)
```

Now define a loop function `printstar` that outputs `n` asterisks (ASCII 42):

```
ready> def printstar(n)
         for i = 1, i < n, 1.0 in
           putchard(42);
Parsed a function definition.
```

Execute `printstar(10)`:

```
ready> printstar(10);
**********Evaluated to 0.000000
ready>
```

Ten asterisks appear on your screen, printed by a loop running natively in machine code!

---

## Active Learning Challenge

<details>
<summary><b>Challenge: Nested Loops to Draw a Triangle</b></summary>

Combine your `for` loop with a newline (ASCII 10) to draw a right-angled triangle of stars:

```
*
**
***
****
*****
```

Write a Kaleidoscope function `triangle(n)` that achieves this using two nested loops.

<br>

**Solution**:
```kaleidoscope
extern putchard(x);

def triangle(n)
  for row = 1, row < n + 1, 1.0 in
    (for col = 0, col < row, 1.0 in
       putchard(42))
    : putchard(10);
```

*(Note: We will introduce the user-defined `:` sequential operator in Section 8.  Alternatively, you can write a helper function `printline(row)` that prints `row` stars and then prints a newline!)*

```kaleidoscope
def printline(n)
  if (for i = 0, i < n, 1 in putchard(42)) == 0 then
    putchard(10)
  else
    0;

def triangle(n)
  for row = 1, row < n + 1, 1 in
    printline(row);
```

</details>

---

Next: [Section 8: User-Defined Operators](../08-user-defined-ops/index.md) - Extend Kaleidoscope to allow custom binary and unary operators.
