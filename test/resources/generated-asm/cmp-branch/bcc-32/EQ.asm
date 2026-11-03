@main_type "default"

@type default
  @method test_EQ(I32, I32)I64
    @code
      @live.prim IR1, IR2
      movi.32 IR3, 0x1
      bcc.32 EQ, IR1, IR2, skip
      subi.32 IR3, IR3, 0x1
      skip:
      ret.64 IR3
    @end
  @end
@end
