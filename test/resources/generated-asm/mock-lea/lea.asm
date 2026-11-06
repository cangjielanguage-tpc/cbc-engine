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
  @method test_lea_rec_f0(I64)I64
    @code
      @live.rec IR1
      lea IR2, IR1, #rec_f0
      @dead IR1
      ret.64 IR2
    @end
  @end

  @method test_lea_rec_f1(I64)I64
    @code
      @live.rec IR1
      lea IR2, IR1, #rec_f1
      @dead IR1
      ret.64 IR2
    @end
  @end

  @method test_lea_obj_x(I64)I64
    @code
      @live.ref IR1
      lea IR2, IR1, #obj_x
      @dead IR1
      ret.64 IR2
    @end
  @end

  @method test_lea_obj_y(I64)I64
    @code
      @live.ref IR1
      lea IR2, IR1, #obj_y
      @dead IR1
      ret.64 IR2
    @end
  @end

  @method test_lea_rec_rt(I64, I64)I64
    @code
      @live.rec IR1
      @live.prim IR2
      lea IR3, IR1, #rec_f0
      st.raw.64 IR2, IR3, 0x0
      ld.field IR4, IR1, #rec_f0
      ld.raw.64 IR5, IR3, 0x0
      scc.64 EQ, IR6, IR4, IR2
      scc.64 EQ, IR7, IR5, IR2
      mul.64 IR6, IR6, IR7
      @dead IR1, IR2, IR3, IR4, IR5, IR7
      ret.64 IR6
    @end
  @end

  @method test_lea_obj_rt(I64, I64)I64
    @code
      @live.ref IR1
      @live.prim IR2
      lea IR3, IR1, #obj_x
      st.raw.64 IR2, IR3, 0x0
      ld.field IR4, IR1, #obj_x
      ld.raw.64 IR5, IR3, 0x0
      scc.64 EQ, IR6, IR4, IR2
      scc.64 EQ, IR7, IR5, IR2
      mul.64 IR6, IR6, IR7
      @dead IR1, IR2, IR3, IR4, IR5, IR7
      ret.64 IR6
    @end
  @end
@end
