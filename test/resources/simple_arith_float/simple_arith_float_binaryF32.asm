@type default {
  @method default test_ADD [ ] F32 {
    fadd.32 FR0, FR0, FR1
    fret.32 FR0
  }

  @method default test_SUB [ ] F32 {
    fsub.32 FR0, FR0, FR1
    fret.32 FR0
  }

  @method default test_MUL [ ] F32 {
    fmul.32 FR0, FR0, FR1
    fret.32 FR0
  }

  @method default test_DIV [ ] F32 {
    fdiv.32 FR0, FR0, FR1
    fret.32 FR0
  }
}