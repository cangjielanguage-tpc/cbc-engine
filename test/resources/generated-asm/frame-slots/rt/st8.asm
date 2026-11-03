@main_type "default"

@type default
  @method test_st8_64(I64)I64
    @untyped_count 0x1
    @code
      @live.prim IR1
      st.uslot.8 IR1, $0
      ld.uslot.64 IR2, $0
      @dead IR1, $0
      ret.64 IR2
    @end
  @end

  @method test_st8_s8(I64)I64
    @untyped_count 0x1
    @code
      @live.prim IR1
      st.uslot.8 IR1, $0
      ld.uslot.s8 IR2, $0
      @dead IR1, $0
      ret.64 IR2
    @end
  @end

  @method test_st8_u8(I64)I64
    @untyped_count 0x1
    @code
      @live.prim IR1
      st.uslot.8 IR1, $0
      ld.uslot.u8 IR2, $0
      @dead IR1, $0
      ret.64 IR2
    @end
  @end
@end
