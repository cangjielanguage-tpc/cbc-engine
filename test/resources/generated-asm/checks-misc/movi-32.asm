@main_type "default"

@type default
  @method test_movi32()I64
    @code
      movi.32 IR2, 0x7
      movi.32 IR3, 0x3
      add.32 IR1, IR2, IR3
      ret.64 IR1
    @end
  @end
@end
