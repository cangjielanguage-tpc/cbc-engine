@main_type "default"

@type default
  @method test_MOV(F64, F64)F64
    @code
      fmov.64 FR0, FR1
      fret.64 FR0
    @end
  @end
@end
