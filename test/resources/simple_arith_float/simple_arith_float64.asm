;strict
@main_type "default"

@type default
  @method test_NEG()F64
    @code
      fneg.64 FR0, FR0
      fret.64 FR0
    @end
  @end

  @method test_SQRT()F64
    @code
      fsqrt.64 FR0, FR0
      fret.64 FR0
    @end
  @end

  @method test_ABS()F64
    @code
      fabs.64 FR0, FR0
      fret.64 FR0
    @end
  @end

  @method test_ADD()F64
    @code
      fadd.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end

  @method test_SUB()F64
    @code
      fsub.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end

  @method test_MUL()F64
    @code
      fmul.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end

  @method test_DIV()F64
    @code
      fdiv.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end
@end
