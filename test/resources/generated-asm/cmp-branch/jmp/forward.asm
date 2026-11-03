@main_type "default"

@type default
  @method test_forward(I64)I64
    @code
      @live.prim IR1
      movi.64 IR2, 0x1
      jmp skip
      movi.64 IR3, 0x7
      ret.64 IR3
      skip:
      ret.64 IR2
    @end
  @end
@end
