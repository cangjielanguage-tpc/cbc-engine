@main_type "default"

@type default
  @method test_LT(I64, I64)I64
    @code
      @live.prim IR1, IR2
      scc.64 LT, IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
