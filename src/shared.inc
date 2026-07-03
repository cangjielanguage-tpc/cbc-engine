
#ifdef __APPLE__
#define Extern(sym) .extern _##sym; .set sym, _##sym
#define DeclareHidden(sym) \
  .global _##sym;          \
  .private_extern _##sym;  \
  .hidden _##sym;          \
  .set sym, _##sym;        \
  _##sym
#else
#define Extern(sym) .extern sym
#define DeclareHidden(sym) \
  .global sym;             \
  .hidden sym;             \
  sym
#endif
