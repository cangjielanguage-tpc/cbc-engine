@main_type "default"

@type default
  @method test_I8_I8(I8)I64
    @code
      @live.prim IR1
      ci2i I8, I8, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
