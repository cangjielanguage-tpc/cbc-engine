@main_type "default"

@type default
  @method test_LOG10(F64)F64
    @code
      flog10.64 FR0, FR0
      fret.64 FR0
    @end
  @end
@end
