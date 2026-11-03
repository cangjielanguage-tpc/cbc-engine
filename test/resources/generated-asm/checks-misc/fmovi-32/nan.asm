@main_type "default"

@type default
  @method test_nan()F32
    @code
      fmovi.32 FR0, nan
      fret.32 FR0
    @end
  @end
@end
