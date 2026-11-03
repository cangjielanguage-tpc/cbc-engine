@main_type "default"

@type default
  @method test_0x20(I64)I64
    @code
      @live.prim IR1
      spowi.32 IR1, IR1, 0x20
      ret.64 IR1
    @end
  @end
@end
