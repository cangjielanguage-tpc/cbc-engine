@main_type "default"

@type default
  @method test_EXP2(F64)F64
    @code
      fexp2.64 FR0, FR0
      fret.64 FR0
    @end
  @end
@end
