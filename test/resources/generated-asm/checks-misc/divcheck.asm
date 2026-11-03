@main_type "default"

@type default
  @method test_divcheck(I64)I64
    @code
      @live.prim IR1
      div.check IR1
      @dead IR1
      movi.64 IR1, 0x2
      ret.64 IR1
    @end
  @end
@end
