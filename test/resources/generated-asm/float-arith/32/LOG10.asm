@main_type "default"

@type default
  @method test_LOG10(F32)F32
    @code
      flog10.32 FR0, FR0
      fret.32 FR0
    @end
  @end
@end
