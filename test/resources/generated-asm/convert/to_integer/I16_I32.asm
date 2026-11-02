@main_type "default"

@type default
  @method main()I64
    @code
      @live.prim IR1
      i2i I32, I16, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
