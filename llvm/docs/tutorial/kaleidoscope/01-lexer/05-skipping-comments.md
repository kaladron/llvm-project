# Step 1.5: Skipping Comments and Active Learning Challenge

In this step, we add the final feature to complete our lexer: ignoring single-line comments.  Kaleidoscope uses `#` to start a comment that runs to the end of the line, just like Python, Bash, and Ruby.

---

## Comment Skipping Mechanics

Because comments exist purely for human readers and carry no syntactic meaning for the parser, `gettok()` discards them completely.

When `gettok()` sees `LastChar == '#'`, it consumes characters in a loop until it reaches a newline (`'\n'`), a carriage return (`'\r'`), or `EOF`.  As long as the input has not ended (`LastChar != EOF`), `gettok()` tail-calls itself (`return gettok();`) to fetch and return the next real token on a subsequent line.

:::{note}
**Leaving Newlines for the Whitespace Loop**:
Notice that the comment loop stops as soon as `LastChar` becomes `'\n'` or `'\r'`, leaving `LastChar` holding that newline character.  When `gettok()` calls itself recursively on the next line, the `while (isspace(LastChar))` loop from [Step 1.2](02-skipping-whitespace.md) immediately swallows that newline (plus any indentation on the next line!) before scanning the next token.
:::

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file and make two quick updates on top of [Step 1.4](04-identifiers-keywords.md).

### 1. Skip `#` Comments in `gettok()`

Insert the comment-skipping block inside `gettok()`, directly after the numeric literal block and right before `if (LastChar == EOF)`:

```cpp
  // Recognise comments: '#' through end of line.
  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }
```

Check both edge cases:
* **Comment followed by more code**: `LastChar` stops on `'\n'`, `LastChar != EOF` is true, and `return gettok();` scans the next line transparently.
* **File ending mid-comment without a trailing newline**: `LastChar` becomes `EOF`, `if (LastChar != EOF)` is false, execution falls through to `if (LastChar == EOF) return tok_eof;`, and the lexer exits cleanly.

### 2. Update the Banner in `main()`

Update the two `printf` lines at the top of `main()` so the inspector announces `Step 1.5: Full Lexer`:

```cpp
  printf("Kaleidoscope Token Inspector (Step 1.5: Full Lexer)\n");
  printf("Type code with comments and press Enter (Ctrl+D to exit):\n\n");
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete reference lexer:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -100,
  tok_extern = -101,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise identifiers and keywords: [a-zA-Z_][a-zA-Z0-9_]*
  if (isalpha(LastChar) || LastChar == '_') {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())) || LastChar == '_')
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

  // Recognise numeric literals: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  // Recognise comments: '#' through end of line.
  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

int main() {
  printf("Kaleidoscope Token Inspector (Step 1.5: Full Lexer)\n");
  printf("Type code with comments and press Enter (Ctrl+D to exit):\n\n");

  int tok;
  while ((tok = gettok()) != tok_eof) {
    switch (tok) {
    case tok_def:
      printf("Token: KEYWORD(def)\n");
      break;
    case tok_extern:
      printf("Token: KEYWORD(extern)\n");
      break;
    case tok_identifier:
      printf("Token: IDENTIFIER(\"%s\")\n", IdentifierStr.c_str());
      break;
    case tok_number:
      printf("Token: NUMBER(%f)\n", NumVal);
      break;
    default:
      printf("Token: '%c' (ASCII %d)\n", static_cast<char>(tok), tok);
      break;
    }
  }

  printf("[EOF]\n");
  return 0;
}
```
</details>

---

## Compiling and Running

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary and enter a multi-line snippet with an inline `#` comment:

```bash
./kaleidoscope
```

```text
Kaleidoscope Token Inspector (Step 1.5: Full Lexer)
Type code with comments and press Enter (Ctrl+D to exit):

def foo(x) # this is a comment
  x + 1;
Token: KEYWORD(def)
Token: IDENTIFIER("foo")
Token: '(' (ASCII 40)
Token: IDENTIFIER("x")
Token: ')' (ASCII 41)
Token: IDENTIFIER("x")
Token: '+' (ASCII 43)
Token: NUMBER(1.000000)
Token: ';' (ASCII 59)
[EOF]
```

Everything from `#` to the end of the first line vanished, and the scanner picked right up on `x + 1;` on the next line!

---

## Active Learning Challenge: Adding Conditional Keywords

Now that you have built the entire lexer by hand, stretch your muscles by adding three new language keywords on your own.

### The Challenge

Later in [Section 6](../06-control-flow/index.md), we will add conditional expressions to Kaleidoscope with this syntax:

```text
if x < 3 then 1 else 2;
```

Temporarily extend your `kaleidoscope.cpp` to recognise three new keywords:
1.  `if` (`tok_if = -102`)
2.  `then` (`tok_then = -103`)
3.  `else` (`tok_else = -104`)

Notice how our reserved enum ranges from [Step 1.3](03-numeric-literals.md) and [Step 1.4](04-identifiers-keywords.md) pay off here: appending `-102`, `-103`, and `-104` to the Commands / Keywords block leaves `tok_identifier = -200` and `tok_number = -201` completely untouched!

### Testing Your Challenge Solution

When you recompile and type `if x then 1 else 2;`, your inspector should print:

```text
Token: KEYWORD(if)
Token: IDENTIFIER("x")
Token: KEYWORD(then)
Token: NUMBER(1.000000)
Token: KEYWORD(else)
Token: NUMBER(2.000000)
Token: ';' (ASCII 59)
```

Try implementing all three edits (`enum Token`, `gettok()`, and `main()`) yourself before peeking at the solution fold below.

<details>
<summary>Click here to reveal the challenge solution</summary>

Here are the three spots to update:

1.  Add the token values to `enum Token`:
```cpp
enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -100,
  tok_extern = -101,
  tok_if = -102,
  tok_then = -103,
  tok_else = -104,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
};
```

2.  Recognise the words inside the identifier block of `gettok()`:
```cpp
    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "then")
      return tok_then;
    if (IdentifierStr == "else")
      return tok_else;
    return tok_identifier;
```

3.  Print them in the `switch` inside `main()`:
```cpp
    case tok_if:
      printf("Token: KEYWORD(if)\n");
      break;
    case tok_then:
      printf("Token: KEYWORD(then)\n");
      break;
    case tok_else:
      printf("Token: KEYWORD(else)\n");
      break;
```
</details>

:::{important}
**Revert the Challenge Keywords Before Starting Section 2!**
Once you have verified your `if`, `then`, and `else` tokens, **remove those three challenge additions** so your `kaleidoscope.cpp` matches the Step 1.5 reference fold above (`tok_def` and `tok_extern` only).  Keeping `enum Token` aligned with the baseline ensures `-Wall -Wextra` stays warning-free throughout Sections 2 through 5, and `if` / `then` / `else` will return officially in [Step 6.1](../06-control-flow/01-if-ast.md)!
:::

---

## Next Steps

Congratulations!  You have written a complete, working lexical scanner from scratch in under ninety lines of C++.

In [Section 2: The Parser and AST](../02-parser-ast/index.md), we will replace the token-printing `main()` loop with a recursive-descent parser that assembles tokens into an in-memory Abstract Syntax Tree.

---

[Previous: Step 1.4 - Identifiers and Keywords](04-identifiers-keywords.md) | [Section 1 Overview](index.md) | [Next: Section 2 - The Parser and AST](../02-parser-ast/index.md)
