#include <cctype>
#include <cstdio>
#include <map>

// BinopPrecedence - Maps operator character to numeric precedence.
static std::map<char, int> BinopPrecedence;

static int CurTok = '+';

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[static_cast<char>(CurTok)];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

int main() {
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  printf("Operator Precedence Table (Step 3.2):\n");
  for (const auto &pair : BinopPrecedence)
    printf("  '%c' => precedence %d\n", pair.first, pair.second);

  printf("Current operator '%c' has precedence: %d\n", CurTok,
         GetTokPrecedence());

  return 0;
}
