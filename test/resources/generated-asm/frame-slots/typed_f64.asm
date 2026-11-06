@main_type "default"

@field_ref rec_x = default:Rec@rec x I64
@field_ref rec_f = default:Rec@rec f F64

@type default:Rec
  @flags PUBLIC RECORD
  @field x I64
    @flags PUBLIC
  @end
  @field f F64
    @flags PUBLIC
  @end
@end

@type default

  @method test_typed_f64(F64)F64
    @typed_slots default:Rec@rec
    @code
      st.typed.f FR0, $0, #rec_f
      ld.typed.f FR0, $0, #rec_f
      fret.64 FR0
    @end
  @end
@end
