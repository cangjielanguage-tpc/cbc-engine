@main_type "default"

@type default
  @method test_0x2(I32)I64
    @code
      @live.prim IR1
      scci.32 TESTNZ, IR1, IR1, 0x2
      ret.64 IR1
    @end
  @end
@end
