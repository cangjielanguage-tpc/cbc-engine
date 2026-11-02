@main_type "default"

@type default
  @method test_MUL(F64, F64)F64
    @code
      fmul.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end
@end
