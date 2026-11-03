@main_type "default"

@type default
  @method test_SUB(F64, F64)F64
    @code
      fsub.64 FR0, FR0, FR1
      fret.64 FR0
    @end
  @end
@end
