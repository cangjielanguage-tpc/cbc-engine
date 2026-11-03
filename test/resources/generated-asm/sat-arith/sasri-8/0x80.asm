@main_type "default"

@type default
  @method test_0x80(I64)I64
    @code
      @live.prim IR1
      sasri.8 IR1, IR1, 0x80
      ret.64 IR1
    @end
  @end
@end
