# Step 1.5: Skipping Comments and Active Learning Challenge

In this step, we add the final feature to complete our lexer: ignoring single-line comments.  Kaleidoscope uses `#` for comments, similar to Python, Bash, or Ruby.

---

## Comment Skipping Mechanics

Single-line comments in Kaleidoscope begin with `#` and extend through the end of the line.  Because comments carry no syntactic meaning for the parser, our lexer discards comment text completely.

When `gettok()` encounters a `#` character, it consumes all subsequent characters until it reaches a newline (`'\n'`), a carriage return (`'\r'`), or the end of the file (`EOF`).  Once the comment line has been consumed, `gettok()` recursively calls itself to return the first meaningful token on the following line.

> [!NOTE]
> **Leaving Newlines for the Scanner**:
> Notice that the loop stops as soon as it hits `'\n'` or `'\r'`, leaving `LastChar` holding that character.  When `gettok()` calls itself recursively, the `while (isspace(LastChar))` loop at the start of `gettok()` immediately consumes that newline as whitespace and proceeds to scan the next token.

---

## Incremental Code Changes

Open your `kaleidoscope.cpp` file.  Insert the comment-skipping block inside `gettok()`, directly after the numeric literal check:

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

Notice what happens:
* If a line contains only a comment, `gettok()` skips to the end of the line, calls itself, and retrieves the token on the next line.
* If a file ends with a comment, `LastChar` becomes `EOF`, so `gettok()` skips the recursive call, falls through, and returns `tok_eof`.

---

## Verifying Your Code

Before compiling, check that your `kaleidoscope.cpp` has all changes in place.

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
  tok_def = -2,
  tok_extern = -3,

  // Primary
  tok_identifier = -4,
  tok_number = -5,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise identifiers and keywords: [a-zA-Z][a-zA-Z0-9]*
  if (isalpha(LastChar)) {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())))
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
  printf("Kaleidoscope Token Inspector (Step 5: Full Lexer)\n");
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
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type a line containing code and a comment:

```text
Kaleidoscope Token Inspector (Step 5: Full Lexer)
Type code with comments and press Enter (Ctrl+D to exit):

def foo(x) # this is a comment
Token: KEYWORD(def)
Token: IDENTIFIER("foo")
Token: '(' (ASCII 40)
Token: IDENTIFIER("x")
Token: ')' (ASCII 41)
```

The comment after `#` was discarded silently, leaving only the tokens.

---

## Active Learning Challenge: Adding New Keywords

Now that you have built the entire lexer by hand, test your understanding by adding new language keywords yourself.

### The Challenge

Later in this tutorial, we will add conditional branches to Kaleidoscope using this syntax:

```text
if x < 3 then 1 else 2;
```

Modify your `kaleidoscope.cpp` to recognise three new keywords:
1.  `if` (returning `tok_if`)
2.  `then` (returning `tok_then`)
3.  `else` (returning `tok_else`)

### Testing Your Changes

When you recompile and type `if x then 1 else 2;`, your binary should print:

```text
Token: KEYWORD(if)
Token: IDENTIFIER("x")
Token: KEYWORD(then)
Token: NUMBER(1.000000)
Token: KEYWORD(else)
Token: NUMBER(2.000000)
Token: ';' (ASCII 59)
```

Try implementing this yourself before opening the solution fold below.

<details>
<summary>Click here to reveal the solution</summary>

Here are the required changes:

1.  Add the new token enumerations to `enum Token`:
```cpp
enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,
};
```

2.  Add checks in `gettok()` inside the identifier block:
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

3.  Add print handlers in `main()`:
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

---

## Next Steps

Congratulations!  You have written a complete, interactive token scanner from scratch.

In **Section 2: The Parser and AST**, we will start assembling these tokens into an in-memory Abstract Syntax Tree, building an interactive tree pretty-printer along the way.
