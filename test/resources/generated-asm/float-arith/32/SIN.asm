@main_type "default"

@type default
  @method test_SIN(F32)F32
    @code
      fsin.32 FR0, FR0
      fret.32 FR0
    @end
  @end
@end
