@main_type "default"

@type default
  @method test_imm8_p2147483648_64()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm 2147483648, $0
      ld.uslot.64 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm8_pattern_64()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm 0x123456789ABCDEF0, $0
      ld.uslot.64 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm8_pattern_32()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm 0x123456789ABCDEF0, $0
      ld.uslot.32 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm8_pattern_s8()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm 0x123456789ABCDEF0, $0
      ld.uslot.s8 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm8_pattern_u8()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm 0x123456789ABCDEF0, $0
      ld.uslot.u8 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm8_smin_64()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -9223372036854775808, $0
      ld.uslot.64 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end
@end
