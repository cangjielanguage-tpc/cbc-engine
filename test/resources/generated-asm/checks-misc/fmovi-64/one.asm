@main_type "default"

@type default
  @method test_one()F64
    @code
      fmovi.64 FR0, 1.5
      fret.64 FR0
    @end
  @end
@end
