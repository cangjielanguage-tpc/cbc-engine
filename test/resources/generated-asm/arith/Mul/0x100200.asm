@main_type "default"

@type default
  @method main()I64
    @code
      @live.prim IR1
      muli.64 IR1, IR1, 0x100200
      ret.64 IR1
    @end
  @end
@end
