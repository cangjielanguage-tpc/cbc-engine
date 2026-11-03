@main_type "default"

@type default
  @method test_wbcci64(I64)I64
    @code
      @live.prim IR1
      movi.64 IR3, 0x1
      w.bcci.64 LT, IR1, 0x7ff, skip
      subi.64 IR3, IR3, 0x1
      skip:
      ret.64 IR3
    @end
  @end
@end
