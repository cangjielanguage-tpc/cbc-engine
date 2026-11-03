@main_type "default"

@type default
  @method test_0xffffffffffffffff(I64)I64
    @code
      @live.prim IR1
      scci.64 LT, IR1, IR1, -0x1
      ret.64 IR1
    @end
  @end
@end
