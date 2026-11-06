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
  @method test_leag_rec_f0(I64)I64
    @code
      @live.ref IR1
      load.type.info IR2, mem:Rec@rec
      lea.g IR3, IR1, IR2, #rec_f0
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end

  @method test_leag_rec_f1(I64)I64
    @code
      @live.ref IR1
      load.type.info IR2, mem:Rec@rec
      lea.g IR3, IR1, IR2, #rec_f1
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end

  @method test_leag_obj_x(I64)I64
    @code
      @live.ref IR1
      load.type.info IR2, mem:Obj@ref
      lea.g IR3, IR1, IR2, #obj_x
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end

  @method test_leag_obj_y(I64)I64
    @code
      @live.ref IR1
      load.type.info IR2, mem:Obj@ref
      lea.g IR3, IR1, IR2, #obj_y
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end

  @method test_leag_rec_rt(I64, I64)I64
    @code
      @live.ref IR1
      @live.prim IR2
      load.type.info IR3, mem:Rec@rec
      lea.g IR4, IR1, IR3, #rec_f0
      @dead IR1
      @live.rec IR1
      st.raw.64 IR2, IR4, 0x0
      ld.field IR5, IR1, #rec_f0
      ld.raw.64 IR6, IR4, 0x0
      scc.64 EQ, IR7, IR5, IR2
      scc.64 EQ, IR8, IR6, IR2
      mul.64 IR7, IR7, IR8
      @dead IR1, IR2, IR3, IR4, IR5, IR6, IR8
      ret.64 IR7
    @end
  @end

  @method test_leag_obj_rt(I64, I64)I64
    @code
      @live.ref IR1
      @live.prim IR2
      load.type.info IR3, mem:Obj@ref
      lea.g IR4, IR1, IR3, #obj_x
      st.raw.64 IR2, IR4, 0x0
      ld.field IR5, IR1, #obj_x
      ld.raw.64 IR6, IR4, 0x0
      scc.64 EQ, IR7, IR5, IR2
      scc.64 EQ, IR8, IR6, IR2
      mul.64 IR7, IR7, IR8
      @dead IR1, IR2, IR3, IR4, IR5, IR6, IR8
      ret.64 IR7
    @end
  @end
@end
