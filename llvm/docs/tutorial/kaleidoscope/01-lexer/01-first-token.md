# Step 1.1: Your First Token (The 90-Second Win)

Welcome to Step 1.1.  The goal of this step is simple: get a running executable in under ninety seconds that reads input and displays tokens.

---

## The Code

Create a new file named `kaleidoscope.cpp` in your favourite editor, and type the following fifteen lines by hand:

```cpp
#include <cstdio>

int main() {
  printf("Kaleidoscope Token Inspector (Step 1)\n");
  printf("Type characters and press Enter (Ctrl+D to exit):\n\n");

  int c;
  while ((c = getchar()) != EOF) {
    if (c != '\n' && c != '\r')
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(c), c);
  }

  printf("[EOF]\n");
  return 0;
}
```

---

## Compiling and Running

Compile the code with Clang using standard warnings:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type `+ * ;` and press Enter:

```text
Kaleidoscope Token Inspector (Step 1)
Type characters and press Enter (Ctrl+D to exit):

+ * ;
Token: '+' (ASCII 43)
Token: ' ' (ASCII 32)
Token: '*' (ASCII 42)
Token: ' ' (ASCII 32)
Token: ';' (ASCII 59)
```

Press `Ctrl+D` (or `Ctrl+Z` on Windows) to send EOF and exit.

Notice what happened: your binary consumed standard input character by character using `getchar()` and printed each character along with its ASCII code.  However, it also treated spaces as tokens.

In [Step 1.2](02-skipping-whitespace.md), we will teach our scanner to ignore whitespace automatically.
