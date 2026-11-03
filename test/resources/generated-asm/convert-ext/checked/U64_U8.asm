@main_type "default"

@type default
  @method test_U64_U8(U8)I64
    @code
      @live.prim IR1
      ci2i.u U8, U64, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
