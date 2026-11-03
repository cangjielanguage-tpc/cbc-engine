@main_type "default"

@type default
  @method test_0xfffe(I64)I64
    @code
      @live.prim IR1
      cdivi.16 IR1, IR1, 0xfffe
      ret.64 IR1
    @end
  @end
@end
