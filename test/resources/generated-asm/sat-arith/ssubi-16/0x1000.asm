@main_type "default"

@type default
  @method test_0x1000(I64)I64
    @code
      @live.prim IR1
      ssubi.16 IR1, IR1, 0x1000
      ret.64 IR1
    @end
  @end
@end
