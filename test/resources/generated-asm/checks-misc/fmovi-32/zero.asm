@main_type "default"

@type default
  @method test_zero()F32
    @code
      fmovi.32 FR0, 0.0
      fret.32 FR0
    @end
  @end
@end
