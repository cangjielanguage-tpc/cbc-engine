@main_type "default"

@type default
  @method test_0xffff(I64)I64
    @code
      @live.prim IR1
      sulsri.16 IR1, IR1, 0xffff
      ret.64 IR1
    @end
  @end
@end
