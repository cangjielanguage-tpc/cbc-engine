@main_type "default"

@type default
  @method test_0xffffffff(I64)I64
    @code
      @live.prim IR1
      smodi.32 IR1, IR1, 0xffffffff
      ret.64 IR1
    @end
  @end
@end
