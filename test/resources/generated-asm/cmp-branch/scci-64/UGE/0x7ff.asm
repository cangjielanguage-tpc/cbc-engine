@main_type "default"

@type default
  @method test_0x7ff(I64)I64
    @code
      @live.prim IR1
      scci.64 UGE, IR1, IR1, 0x7ff
      ret.64 IR1
    @end
  @end
@end
