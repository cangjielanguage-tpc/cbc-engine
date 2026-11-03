@main_type "default"

@type default
  @method test_fmovi_neginf32()F32
    @code
      fmovi.32 FR0, inf
      fmovi.32 FR1, -1.0
      fmul.32 FR0, FR0, FR1
      fret.32 FR0
    @end
  @end
@end
