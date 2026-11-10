@main_type "default"

@type default
  @method test_FLOOR(F32)F32
    @code
      ffloor.32 FR0, FR0
      fret.32 FR0
    @end
  @end
@end
