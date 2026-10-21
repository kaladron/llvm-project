# Step 8.4: The Mandelbrot Set

To celebrate the power of user-defined operators, control flow, loops, and JIT compilation, we will now build a real Kaleidoscope application: a text-mode renderer for the **Mandelbrot set** fractal.

Notice how much of the "language standard library" is implemented in pure Kaleidoscope code!

---

## 1. Defining Standard Library Operators

Launch your compiler (`./kaleidoscope`) and define the following foundational operators:

```kaleidoscope
extern putchard(x);

# Unary negation:
def unary-(v) 0 - v;

# Sequencing operator ':': evaluates LHS for side-effects, returns RHS
def binary: 1 (x y) y;

# Greater-than operator:
def binary> 10 (LHS RHS) RHS < LHS;

# Logical OR operator:
def binary| 5 (LHS RHS)
  if LHS then 1
  else if RHS then 1
  else 0;
```

---

## 2. Implementing the Mandelbrot Algorithm

Next, define the convergence test.  For each complex coordinate $(c_r, c_i)$, we iterate $z_{k+1} = z_k^2 + c$ up to 255 times, or until $|z|^2 > 4$:

```kaleidoscope
def mandelconverger(real imag iters creal cimag)
  if iters > 255 | (real*real + imag*imag > 4) then
    iters
  else
    mandelconverger(real*real - imag*imag + creal,
                    2*real*imag + cimag,
                    iters + 1, creal, cimag);

def mandelconverge(real imag)
  mandelconverger(real, imag, 0, real, imag);
```

Define a character palette mapping escape iteration counts to ASCII density characters:

```kaleidoscope
def printdensity(d)
  if d > 8 then
    putchard(32)   # space
  else if d > 4 then
    putchard(46)   # '.'
  else if d > 2 then
    putchard(43)   # '+'
  else
    putchard(42);  # '*'
```

Finally, write the 2D rendering loop scanning across the complex plane:

```kaleidoscope
def mandel(xmin xmax xstep ymin ymax ystep)
  for y = ymin, y < ymax, ystep in (
    (for x = xmin, x < xmax, xstep in
       printdensity(mandelconverge(x, y)))
    : putchard(10)
  );
```

---

## 3. Rendering the Fractal

Execute `mandel` over the region $[-2.0, 1.0] \times [-1.0, 1.0]$:

```kaleidoscope
mandel(-2.0, 1.0, 0.05, -1.0, 1.0, 0.1);
```

Your terminal will print the Mandelbrot fractal:

```
***************************++++++++.. .  +++++***************
************************++++++++++...   ..++++++*************
**********************+++++++++....      ..++++++************
*******************++++++++.......        ..... +++**********
*****************++++++++...                     ++**********
**************+++++++++....                     ..++*********
*********+++++. ..........                      . +++********
****+++++++++..        ..                        ..++********
**++++++++++...                                  .++++*******
*+++++......                                    ..++++*******
                                               ...++++*******
*+++++......                                    ..++++*******
**++++++++++...                                  .++++*******
****+++++++++..        ..                        ..++********
*********+++++. ..........                      . +++********
**************+++++++++....                     ..++*********
*****************++++++++...                     ++**********
*******************++++++++.......        ..... +++**********
**********************+++++++++....      ..++++++************
************************++++++++++...   ..++++++*************
***************************++++++++.. .  +++++***************
******************************++++++ .+++++******************
Evaluated to 0.000000
```

Every floating-point operation, recursive function call, user-defined operator, and character output ran as native x86-64 machine instructions generated on the fly by your compiler.

---

Next: [Section 9: Local Variables and Mutability](../09-local-variables/README.md) - Add mutable stack variables with `alloca` and `mem2reg`.
