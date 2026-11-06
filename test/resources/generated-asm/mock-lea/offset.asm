@main_type "default"
@field_ref rec_f0 = mem:Rec@rec f0 I64
@field_ref rec_f1 = mem:Rec@rec f1 I64
@field_ref obj_x = mem:Obj@ref x I64
@field_ref obj_y = mem:Obj@ref y I64
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
  @field f1 I64
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field x I64
    @flags PUBLIC
  @end
  @field y I64
    @flags PUBLIC
  @end
@end

@type default
  @method test_offset_rec_f0()I64
    @code
      load.type.info IR2, mem:Rec@rec
      offset IR1, #rec_f0, IR2
      ret.64 IR1
    @end
  @end

  @method test_offset_rec_f1()I64
    @code
      load.type.info IR2, mem:Rec@rec
      offset IR1, #rec_f1, IR2
      ret.64 IR1
    @end
  @end

  @method test_offset_obj_x()I64
    @code
      load.type.info IR2, mem:Obj@ref
      offset IR1, #obj_x, IR2
      ret.64 IR1
    @end
  @end

  @method test_offset_obj_y()I64
    @code
      load.type.info IR2, mem:Obj@ref
      offset IR1, #obj_y, IR2
      ret.64 IR1
    @end
  @end

  @method test_add_offset_rec_f1(I64)I64
    @code
      @live.prim IR1
      load.type.info IR2, mem:Rec@rec
      add.offset IR1, #rec_f1, IR2
      ret.64 IR1
    @end
  @end

  @method test_add_offset_obj_x(I64)I64
    @code
      @live.prim IR1
      load.type.info IR2, mem:Obj@ref
      add.offset IR1, #obj_x, IR2
      ret.64 IR1
    @end
  @end

  @method test_add_offset_rec_rt(I64, I64)I64
    @code
      @live.prim IR1, IR2
      load.type.info IR3, mem:Rec@rec
      add.offset IR1, #rec_f1, IR3
      st.raw.64 IR2, IR1, 0x0
      ld.raw.64 IR4, IR1, 0x0
      scc.64 EQ, IR5, IR4, IR2
      @dead IR1, IR2, IR3, IR4
      ret.64 IR5
    @end
  @end

  @method test_add_offset_obj_rt(I64, I64)I64
    @code
      @live.prim IR1, IR2
      load.type.info IR3, mem:Obj@ref
      add.offset IR1, #obj_x, IR3
      st.raw.64 IR2, IR1, 0x0
      ld.raw.64 IR4, IR1, 0x0
      scc.64 EQ, IR5, IR4, IR2
      @dead IR1, IR2, IR3, IR4
      ret.64 IR5
    @end
  @end
@end
