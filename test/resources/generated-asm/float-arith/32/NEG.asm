@main_type "default"

@type default
  @method test_NEG(F32)F32
    @code
      fneg.32 FR0, FR0
      fret.32 FR0
    @end
  @end
@end
