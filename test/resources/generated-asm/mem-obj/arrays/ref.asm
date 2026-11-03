@main_type "default"

@type default
  @method test_ref(I64, I64, I64)I64
    @code
      @live.ref IR1, IR3
      @live.prim IR2
      mov.ref IR4, IR3
      @dead IR4
      st.arr.ref IR4, IR1, IR2
      ld.arr.ref IR5, IR1, IR2
      @dead IR1, IR2, IR3
      ret.ref IR5
    @end
  @end
@end
