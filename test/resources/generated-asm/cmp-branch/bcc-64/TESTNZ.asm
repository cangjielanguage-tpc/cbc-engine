@main_type "default"

@type default
  @method test_TESTNZ(I64, I64)I64
    @code
      @live.prim IR1, IR2
      movi.64 IR3, 0x1
      bcc.64 TESTNZ, IR1, IR2, skip
      subi.64 IR3, IR3, 0x1
      skip:
      ret.64 IR3
    @end
  @end
@end
