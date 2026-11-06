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

  @method test_stack_rec(I64)I64
    @typed_slots default:Rec@rec
    @code
      @live.prim IR1
      ld.stack.rec IR2, $0
      st.field IR1, IR2, #rec_x
      @dead IR1
      ld.field IR1, IR2, #rec_x
      ret.64 IR1
    @end
  @end
@end
