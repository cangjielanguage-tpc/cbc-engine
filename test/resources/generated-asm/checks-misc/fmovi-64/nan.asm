@main_type "default"

@type default
  @method test_nan()F64
    @code
      fmovi.64 FR0, nan
      fret.64 FR0
    @end
  @end
@end
