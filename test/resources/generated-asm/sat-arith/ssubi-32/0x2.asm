@main_type "default"

@type default
  @method test_0x2(I64)I64
    @code
      @live.prim IR1
      ssubi.32 IR1, IR1, 0x2
      ret.64 IR1
    @end
  @end
@end
