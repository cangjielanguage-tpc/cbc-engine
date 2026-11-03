@main_type "default"

@type default
  @method test_I32_I32(I32)I64
    @code
      @live.prim IR1
      ci2i I32, I32, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
