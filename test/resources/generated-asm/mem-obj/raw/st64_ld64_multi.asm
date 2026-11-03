@main_type "default"

@type default
  @method test_st64_ld64_multi(I64, I64)I64
    @code
      @live.prim IR1, IR2
      st.raw.64 IR2, IR1, 0x8
      ld.raw.64 IR3, IR1, 0x8
      @dead IR3
      st.raw.64 IR2, IR1, 0x18
      ld.raw.64 IR3, IR1, 0x18
      @dead IR3
      st.raw.64 IR2, IR1, 0x1000
      ld.raw.64 IR3, IR1, 0x1000
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end
@end
