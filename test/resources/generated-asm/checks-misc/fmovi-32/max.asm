@main_type "default"

@type default
  @method test_max()F32
    @code
      fmovi.32 FR0, 340282346638528859811704183484516925440.0
      fret.32 FR0
    @end
  @end
@end
