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
}