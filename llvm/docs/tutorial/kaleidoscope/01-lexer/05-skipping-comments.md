# Step 1.5: Skipping Comments and Active Learning Challenge

In this step, we add the final feature to complete our lexer: ignoring single-line comments.  Kaleidoscope uses `#` for comments, similar to Python, Bash, or Ruby.

---

## Comment Skipping

When `gettok()` encounters a `#` character, it skips all characters until it reaches a newline (`'\n'`), a carriage return (`'\r'`), or the end of the file (`EOF`).

Once the comment is consumed, `gettok()` recursively calls itself so that it returns the next valid token on the subsequent line:

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

---

## Adding Comment Handling

Open `kaleidoscope.cpp` and insert the comment check right after the numeric literal block in `gettok()`:

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
