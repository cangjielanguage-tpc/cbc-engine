@main_type "default"

@type default
  @method test_U16_U16(U16)I64
    @code
      @live.prim IR1
      ci2i.u U16, U16, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
