@main_type "default"

@type default
  @method test_imm2_p128_64()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm 128, $0
      ld.uslot.64 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm2_m4660_64()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -0x1234, $0
      ld.uslot.64 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm2_m4660_32()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -0x1234, $0
      ld.uslot.32 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm2_m4660_s16()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -0x1234, $0
      ld.uslot.s16 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm2_m4660_u16()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -0x1234, $0
      ld.uslot.u16 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm2_m4660_s8()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -0x1234, $0
      ld.uslot.s8 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end

  @method test_imm2_m4660_u8()I64
    @untyped_count 0x1
    @code
      @live.prim $0
      st.uslot.imm -0x1234, $0
      ld.uslot.u8 IR1, $0
      @dead $0
      ret.64 IR1
    @end
  @end
@end
