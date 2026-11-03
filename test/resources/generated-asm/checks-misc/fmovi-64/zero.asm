@main_type "default"

@type default
  @method test_zero()F64
    @code
      fmovi.64 FR0, 0.0
      fret.64 FR0
    @end
  @end
@end
