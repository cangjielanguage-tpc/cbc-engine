@main_type "default"

@type default
  @method test_FPOWI(F64, I64)F64
    @code
      @live.prim IR1
      fpowi.64 FR0, FR1, IR1
      fret.64 FR0
    @end
  @end
@end
