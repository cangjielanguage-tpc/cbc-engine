@main_type "default"

@type default
  @method test_fmovi_neginf64()F64
    @code
      fmovi.64 FR0, inf
      fmovi.64 FR1, -1.0
      fmul.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end
@end
