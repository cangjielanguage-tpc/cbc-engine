@main_type "default"

@type default
  @method test_U8_I16(I16)I64
    @code
      @live.prim IR1
      ci2i.u I16, U8, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
