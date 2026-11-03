@main_type "default"

@type default
  @method test_0xffffffff(I32)I64
    @code
      @live.prim IR1
      movi.32 IR3, 0x1
      bcci.32 EQ, IR1, -0x1, skip
      subi.32 IR3, IR3, 0x1
      skip:
      ret.64 IR3
    @end
  @end
@end
