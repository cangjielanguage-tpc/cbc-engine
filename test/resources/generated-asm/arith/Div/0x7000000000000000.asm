@main_type "default"

@type default
  @method main()I64
    @code
      @live.prim IR1
      divi.64 IR1, IR1, 0x7000000000000000
      ret.64 IR1
    @end
  @end
@end
