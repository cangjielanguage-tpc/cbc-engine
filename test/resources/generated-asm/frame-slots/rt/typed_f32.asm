@main_type "default"

@field_ref recf_f32 = default:RecF@rec f32 F32

@type default:RecF
  @flags PUBLIC RECORD
  @field f32 F32
    @flags PUBLIC
  @end
@end

@type default

  @method test_typed_f32(F32)F32
    @typed_slots default:RecF@rec
    @code
      st.typed.f FR0, $0, #recf_f32
      ld.typed.f FR0, $0, #recf_f32
      fret.32 FR0
    @end
  @end

  @method test_typed_f32_load()F32
    @typed_slots default:RecF@rec
    @code
      ld.typed.f FR0, $0, #recf_f32
      fret.32 FR0
    @end
  @end
@end
