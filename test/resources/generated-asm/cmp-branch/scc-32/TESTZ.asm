@main_type "default"

@type default
  @method test_TESTZ(I32, I32)I64
    @code
      @live.prim IR1, IR2
      scc.32 TESTZ, IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
