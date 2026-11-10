@main_type "default"

@type default
  @method test_FPOWI(F32, I64)F32
    @code
      @live.prim IR1
      fpowi.32 FR0, FR1, IR1
      fret.32 FR0
    @end
  @end
@end
