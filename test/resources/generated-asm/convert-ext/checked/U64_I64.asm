@main_type "default"

@type default
  @method test_U64_I64(I64)I64
    @code
      @live.prim IR1
      ci2i.u I64, U64, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
