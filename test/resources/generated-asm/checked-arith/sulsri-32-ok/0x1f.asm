@main_type "default"

@type default
  @method test_0x1f(I64)I64
    @code
      @live.prim IR1
      sulsri.32 IR1, IR1, 0x1f
      ret.64 IR1
    @end
  @end
@end
