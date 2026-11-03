@main_type "default"

@type default
  @method test_ssub_8(I64, I64)I64
    @code
      @live.prim IR1, IR2
      ssub.8 IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
