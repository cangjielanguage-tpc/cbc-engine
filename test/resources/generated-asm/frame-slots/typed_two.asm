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

  @method test_typed_two(I64, I64)I64
    @typed_slots default:Rec@rec, default:Rec@rec
    @code
      @live.prim IR1, IR2
      st.typed IR1, $0, #rec_x
      st.typed IR2, $1, #rec_x
      @dead IR1, IR2
      ld.typed IR1, $0, #rec_x
      ld.typed IR2, $1, #rec_x
      add.64 IR1, IR1, IR2
      ret.64 IR1
    @end
  @end
@end
