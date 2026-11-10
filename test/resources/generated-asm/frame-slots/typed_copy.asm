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

@field_ref recc_c = default:RecC@rec c I64
@field_ref recc_d = default:RecC@rec d F32

@type default:RecC
  @flags PUBLIC RECORD
  @field c I64
    @flags PUBLIC
  @end
  @field d F32
    @flags PUBLIC
  @end
@end

@type default

  @method test_typed_copy(I64)I64
    @typed_slots default:Rec@rec, default:Rec@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #rec_x
      @dead IR1
      copy.typed $1, $0
      ld.typed IR1, $1, #rec_x
      ld.typed IR3, $0, #rec_x
      add.64 IR1, IR1, IR3
      ret.64 IR1
    @end
  @end

  @method test_typed_copy_pad(I64)I64
    @typed_slots default:RecC@rec, default:RecC@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #recc_c
      @dead IR1
      copy.typed $1, $0
      ld.typed IR1, $1, #recc_c
      ret.64 IR1
    @end
  @end

  @method test_typed_copy_g(I64)I64
    @typed_slots default:Rec@rec, default:Rec@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #rec_x
      @dead IR1
      load.type.info IR3, default:Rec@rec
      copy.typed.g $1, $0, IR3
      @dead IR3
      ld.typed IR1, $1, #rec_x
      ld.typed IR3, $0, #rec_x
      add.64 IR1, IR1, IR3
      ret.64 IR1
    @end
  @end

  @method test_typed_copy_to(I64)I64
    @typed_slots default:Rec@rec, default:Rec@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #rec_x
      @dead IR1
      ld.stack.rec IR2, $0
      copy.to.typed $1, IRZ, IR2
      ld.typed IR1, $1, #rec_x
      ld.typed IR4, $0, #rec_x
      add.64 IR1, IR1, IR4
      ret.64 IR1
    @end
  @end

  @method test_typed_copy_from(I64)I64
    @typed_slots default:Rec@rec, default:Rec@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #rec_x
      @dead IR1
      ld.stack.rec IR2, $1
      copy.from.typed IRZ, IR2, $0
      ld.typed IR1, $1, #rec_x
      ld.typed IR4, $0, #rec_x
      add.64 IR1, IR1, IR4
      ret.64 IR1
    @end
  @end

  @method test_typed_copy_to_g(I64)I64
    @typed_slots default:Rec@rec, default:Rec@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #rec_x
      @dead IR1
      ld.stack.rec IR2, $0
      load.type.info IR3, default:Rec@rec
      copy.to.typed.g $1, IRZ, IR2, IR3
      @dead IR3
      ld.typed IR1, $1, #rec_x
      ld.typed IR4, $0, #rec_x
      add.64 IR1, IR1, IR4
      ret.64 IR1
    @end
  @end

  @method test_typed_copy_from_g(I64)I64
    @typed_slots default:Rec@rec, default:Rec@rec
    @code
      @live.prim IR1
      st.typed IR1, $0, #rec_x
      @dead IR1
      ld.stack.rec IR2, $1
      load.type.info IR3, default:Rec@rec
      copy.from.typed.g IRZ, IR2, $0, IR3
      @dead IR3
      ld.typed IR1, $1, #rec_x
      ld.typed IR4, $0, #rec_x
      add.64 IR1, IR1, IR4
      ret.64 IR1
    @end
  @end
@end
