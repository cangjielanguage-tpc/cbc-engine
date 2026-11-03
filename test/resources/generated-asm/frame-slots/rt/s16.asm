@main_type "default"

@type default
  @method test_s16(I64)I64
    @untyped_count 0x1
    @code
      @live.prim IR1
      st.uslot.64 IR1, $0
      ld.uslot.s16 IR2, $0
      @dead IR1, $0
      ret.64 IR2
    @end
  @end
@end
