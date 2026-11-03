@main_type "default"

@type default
  @method test_0x3(I64)I64
    @code
      @live.prim IR1
      clsri.8 IR1, IR1, 0x3
      ret.64 IR1
    @end
  @end
@end
