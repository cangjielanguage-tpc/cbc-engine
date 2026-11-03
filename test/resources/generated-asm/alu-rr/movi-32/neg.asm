@main_type "default"

@type default
  @method test_neg(I64)I64
    @code
      @live.prim IR1
      movi.32 IR2, 0x0
      sub.32 IR1, IR2, IR1
      ret.64 IR1
    @end
  @end
@end
