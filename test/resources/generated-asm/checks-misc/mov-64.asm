@main_type "default"

@type default
  @method test_mov64(I64)I64
    @code
      @live.prim IR1
      mov.64 IR2, IR1
      ret.64 IR2
    @end
  @end
@end
