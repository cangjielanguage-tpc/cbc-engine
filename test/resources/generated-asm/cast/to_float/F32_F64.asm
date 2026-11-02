@main_type "default"

@type default
  @method test_F32_F64(F64)F32
    @code
      @live.prim IR1
      f2f F64, F32, FR0, FR0
      fret.32 FR0
    @end
  @end
@end
