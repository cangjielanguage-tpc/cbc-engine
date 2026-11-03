@main_type "default"

@type default
  @method test_0x7f(I64)I64
    @code
      @live.prim IR1
      smodi.8 IR1, IR1, 0x7f
      ret.64 IR1
    @end
  @end
@end
