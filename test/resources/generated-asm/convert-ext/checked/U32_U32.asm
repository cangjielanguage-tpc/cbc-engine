@main_type "default"

@type default
  @method test_U32_U32(U32)I64
    @code
      @live.prim IR1
      ci2i.u U32, U32, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
