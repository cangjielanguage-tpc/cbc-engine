@main_type "default"

@type default
  @method test_EXP2(F32)F32
    @code
      fexp2.32 FR0, FR0
      fret.32 FR0
    @end
  @end
@end
