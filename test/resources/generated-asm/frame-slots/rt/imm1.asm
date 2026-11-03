@main_type "default"

@type default
  @method test_imm1_m1_64()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -1, $0
      ld.uslot.64 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm1_m1_32()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -1, $0
      ld.uslot.32 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm1_m1_s8()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -1, $0
      ld.uslot.s8 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm1_m1_u8()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -1, $0
      ld.uslot.u8 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm1_p127_64()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm 127, $0
      ld.uslot.64 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end
@end
