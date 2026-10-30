;strict
@main_type "default"

@type default
  @method main()I64
    @code
      movi.64 IR1, 0x1
      i2i I32, I64, IR1, IR1
      i2i.u U64, U32, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
