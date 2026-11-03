@main_type "default"

@field_ref recn_i8 = default:RecN@rec i8 I8
@field_ref recn_u8 = default:RecN@rec u8 U8
@field_ref recn_i16 = default:RecN@rec i16 I16
@field_ref recn_u16 = default:RecN@rec u16 U16
@field_ref recn_f32 = default:RecN@rec f32 F32

@type default:RecN
  @flags PUBLIC RECORD
  @field i8 I8
    @flags PUBLIC
  @end
  @field u8 U8
    @flags PUBLIC
  @end
  @field i16 I16
    @flags PUBLIC
  @end
  @field u16 U16
    @flags PUBLIC
  @end
  @field f32 F32
    @flags PUBLIC
  @end
@end

@type default

  @method test_typed_narrow_i8(I64)I64
    @typed_slots default:RecN@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #recn_i8
      @dead IR1
      ld.typed IR1, $0, #recn_i8
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_u8(I64)I64
    @typed_slots default:RecN@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #recn_u8
      @dead IR1
      ld.typed IR1, $0, #recn_u8
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_i16(I64)I64
    @typed_slots default:RecN@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #recn_i16
      @dead IR1
      ld.typed IR1, $0, #recn_i16
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_u16(I64)I64
    @typed_slots default:RecN@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #recn_u16
      @dead IR1
      ld.typed IR1, $0, #recn_u16
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_all_store()I64
    @typed_slots default:RecN@rec
    @code
      movi.64 IR1, 0xa5
      movi.64 IR2, 0x5a1c
      movi.64 IR3, 0x8163
      movi.64 IR4, 0x7211
      st.typed IR1, $0, #recn_i8
      st.typed IR2, $0, #recn_u8
      st.typed IR3, $0, #recn_i16
      st.typed IR4, $0, #recn_u16
      @dead IR1, IR2, IR3, IR4
      fmovi.32 FR0, 1.5
      st.typed.f FR0, $0, #recn_f32
      ld.typed IR1, $0, #recn_i16
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_load_i8()I64
    @typed_slots default:RecN@rec
    @code
      ld.typed IR1, $0, #recn_i8
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_load_u8()I64
    @typed_slots default:RecN@rec
    @code
      ld.typed IR1, $0, #recn_u8
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_load_i16()I64
    @typed_slots default:RecN@rec
    @code
      ld.typed IR1, $0, #recn_i16
      ret.64 IR1
    @end
  @end

  @method test_typed_narrow_load_u16()I64
    @typed_slots default:RecN@rec
    @code
      ld.typed IR1, $0, #recn_u16
      ret.64 IR1
    @end
  @end
@end
