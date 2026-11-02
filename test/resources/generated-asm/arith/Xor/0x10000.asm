@main_type "default"

@type default
  @method main()I64
    @code
      @live.prim IR1
      xori.64 IR1, IR1, 0x10000
      ret.64 IR1
    @end
  @end
@end
