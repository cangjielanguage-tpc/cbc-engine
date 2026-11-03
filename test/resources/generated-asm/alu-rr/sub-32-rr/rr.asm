@main_type "default"

@type default
  @method test_sub_32(I64, I64)I64
    @code
      @live.prim IR1, IR2
      sub.32 IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
