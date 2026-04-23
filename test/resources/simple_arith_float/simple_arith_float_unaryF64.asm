@type default {

  @method default test_NEG [ ] F64 {
    fneg.64 FR0, FR0
    fret.64 FR0
  }

  @method default test_SQRT [ ] F64 {
    fsqrt.64 FR0, FR0
    fret.64 FR0
  }

  @method default test_ABS [ ] F64 {
    fabs.64 FR0, FR0
    fret.64 FR0
  }
}