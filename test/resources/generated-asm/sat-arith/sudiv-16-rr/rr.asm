@main_type "default"

@type default
  @method test_sudiv_16(I64, I64)I64
    @code
      @live.prim IR1, IR2
      sudiv.16 IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
