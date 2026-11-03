@main_type "default"

@type default
  @method test_0x7ff(I64)I64
    @code
      @live.prim IR1
      movi.64 IR3, 0x1
      bcci.64 NE, IR1, 0x7ff, skip
      subi.64 IR3, IR3, 0x1
      skip:
      ret.64 IR3
    @end
  @end
@end
