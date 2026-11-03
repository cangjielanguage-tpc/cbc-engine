@main_type "default"

@type default
  @method test_0x1(I32)I64
    @code
      @live.prim IR1
      scci.32 NE, IR1, IR1, 0x1
      ret.64 IR1
    @end
  @end
@end
