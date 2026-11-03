@main_type "default"

@type default
  @method test_0xffffffffffffffff(I64)I64
    @code
      @live.prim IR1
      sumuli.64 IR1, IR1, 0xffffffffffffffff
      ret.64 IR1
    @end
  @end
@end
