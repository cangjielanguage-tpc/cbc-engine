@main_type "default"

@type default
  @method test_0xf000000100000001(I64)I64
    @code
      @live.prim IR1
      udivi.64 IR1, IR1, 0xf000000100000001
      ret.64 IR1
    @end
  @end
@end
