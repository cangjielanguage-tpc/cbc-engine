@main_type "default"

@type default
  @method test_ref(I64)I64
    @untyped_count 0x1
    @code
      @live.ref IR1
      st.uslot.ref IR1, $0
      ld.uslot.ref IR2, $0
      @dead IR1, $0
      ret.ref IR2
    @end
  @end
@end
