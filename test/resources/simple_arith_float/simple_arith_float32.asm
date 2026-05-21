@main_type default

@type default {
  @method default test_NEG [ ] F32 {
    fneg.32 FR0, FR0
    fret.32 FR0
  }

  @method default test_SQRT [ ] F32 {
    fsqrt.32 FR0, FR0
    fret.32 FR0
  }

  @method default test_ABS [ ] F32 {
    fabs.32 FR0, FR0
    fret.32 FR0
  }

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
