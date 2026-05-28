;strict
@main_type "default"

@type default
  @method test_NEG()F32
    @code
      fneg.32 FR0, FR0
      fret.32 FR0
    @end
  @end

  @method test_SQRT()F32
    @code
      fsqrt.32 FR0, FR0
      fret.32 FR0
    @end
  @end

  @method test_ABS()F32
    @code
      fabs.32 FR0, FR0
      fret.32 FR0
    @end
  @end

  @method test_ADD()F32
    @code
      fadd.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end

  @method test_SUB()F32
    @code
      fsub.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end

  @method test_MUL()F32
    @code
      fmul.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end

  @method test_DIV()F32
    @code
      fdiv.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end
@end
