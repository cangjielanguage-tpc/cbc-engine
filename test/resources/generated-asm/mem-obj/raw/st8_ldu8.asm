@main_type "default"

@type default
  @method test_st8_ldu8(I64, I64)I64
    @code
      @live.prim IR1, IR2
      st.raw.8 IR2, IR1, 0x8
      ld.raw.u8 IR3, IR1, 0x8
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end
@end
