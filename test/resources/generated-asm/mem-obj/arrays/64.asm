@main_type "default"

@type default
  @method test_64(I64, I64, I64)I64
    @code
      @live.ref IR1
      @live.prim IR2, IR3
      @dead IR3
      st.arr.64 IR3, IR1, IR2
      @dead IR3
      ld.arr.64 IR4, IR1, IR2
      @dead IR1, IR2
      ret.64 IR4
    @end
  @end
@end
