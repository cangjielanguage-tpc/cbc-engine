;strict
@main_type "default"

@type default
  @method cj_entry()I64
    @code
      movi.64 IR2, 0x5
      movi.64 IR3, 0x7
      addi.64 IR2, IR2, 0x14
      xori.64 IR1, IR3, 0x2
      @dead IR3
      div.64 IR3, IR2, IR1
      @dead IR1
      subi.64 IR1, IR3, 0x4
      ret.64 IR1
    @end
  @end
@end
