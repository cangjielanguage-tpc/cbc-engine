@main_type "default"

@type default
  @method test_0xfffffffffffff800(I64)I64
    @code
      @live.prim IR1
      scci.64 GE, IR1, IR1, -0x800
      ret.64 IR1
    @end
  @end
@end
