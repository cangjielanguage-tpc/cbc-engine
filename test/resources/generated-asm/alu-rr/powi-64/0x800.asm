@main_type "default"

@type default
  @method test_0x800(I64)I64
    @code
      @live.prim IR1
      powi.64 IR1, IR1, 0x800
      ret.64 IR1
    @end
  @end
@end
