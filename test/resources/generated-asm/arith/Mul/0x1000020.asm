@main_type "default"

@type default
  @method test_0x1000020(I64)I64
    @code
      @live.prim IR1
      muli.64 IR1, IR1, 0x1000020
      ret.64 IR1
    @end
  @end
@end
