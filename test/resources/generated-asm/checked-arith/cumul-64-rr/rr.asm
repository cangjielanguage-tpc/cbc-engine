@main_type "default"

@type default
  @method test_cumul_64(I64, I64)I64
    @code
      @live.prim IR1, IR2
      cumul.64 IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
