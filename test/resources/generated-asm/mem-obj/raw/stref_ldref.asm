@main_type "default"

@type default
  @method test_stref_ldref(I64, I64)I64
    @code
      @live.prim IR1
      @live.ref IR2
      st.raw.ref IR2, IR1, 0x8
      ld.raw.ref IR3, IR1, 0x8
      @dead IR1, IR2
      ret.ref IR3
    @end
  @end
@end
