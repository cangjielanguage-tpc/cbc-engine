@type default {
  @method default test_ADD [ ] F64 {
    fadd.64 FR0, FR0, FR1
    fret.64 FR0
  }

  @method default test_SUB [ ] F64 {
    fsub.64 FR0, FR0, FR1
    fret.64 FR0
  }

  @method default test_MUL [ ] F64 {
    fmul.64 FR0, FR0, FR1
    fret.64 FR0
  }

  @method default test_DIV [ ] F64 {
    fdiv.64 FR0, FR0, FR1
    fret.64 FR0
  }
}