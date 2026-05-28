;strict
@main_type "default"

@type default
  @method main()I64
    @code
      movi.64 IR3, 0x1
      movi.64 IR1, 0x0
      movi.64 IR2, 0x7
      sub.64 IR2, IR2, IR3
      @dead IR3
      add.64 IR3, IR1, IR2
      @dead IR1
      mul.64 IR1, IR2, IR3
      ret.64 IR1
    @end
  @end
@end
