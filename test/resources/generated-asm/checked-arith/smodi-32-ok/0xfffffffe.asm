@main_type "default"

@type default
  @method test_0xfffffffe(I64)I64
    @code
      @live.prim IR1
      smodi.32 IR1, IR1, 0xfffffffe
      ret.64 IR1
    @end
  @end
@end
