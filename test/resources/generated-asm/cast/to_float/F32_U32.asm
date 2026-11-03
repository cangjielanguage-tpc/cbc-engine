@main_type "default"

@type default
  @method test_F32_U32(U32)F32
    @code
      @live.prim IR1
      i2f U32, F32, IR1, FR0
      fret.32 FR0
    @end
  @end
@end
