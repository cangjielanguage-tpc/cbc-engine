@main_type "default"

@type default
  @method test_POW(F32, F32)F32
    @code
      fpow.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end
@end
