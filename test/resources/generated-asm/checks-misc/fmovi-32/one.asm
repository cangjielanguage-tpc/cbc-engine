@main_type "default"

@type default
  @method test_one()F32
    @code
      fmovi.32 FR0, 1.5
      fret.32 FR0
    @end
  @end
@end
