@main_type "default"

@type default
  @method test_F64_U64(U64)F64
    @code
      @live.prim IR1
      i2f U64, F64, IR1, FR0
      fret.64 FR0
    @end
  @end
@end
