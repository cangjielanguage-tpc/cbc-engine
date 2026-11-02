@main_type "default"

@type default
  @method main()I64
    @code
      @live.prim IR1
      xori.64 IR1, IR1, 0x7000000010000001
      ret.64 IR1
    @end
  @end
@end
