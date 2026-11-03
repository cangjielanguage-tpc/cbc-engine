@main_type "default"
@field_ref rec_f0 = mem:Rec@rec f0 I64
@field_ref rec_f1 = mem:Rec@rec f1 I32
@field_ref rec_f2 = mem:Rec@rec f2 I16
@field_ref rec_f3 = mem:Rec@rec f3 I8
@field_ref rec_f4 = mem:Rec@rec f4 U8
@field_ref rec_f5 = mem:Rec@rec f5 U16
@field_ref rec_f6 = mem:Rec@rec f6 F32
@field_ref rec_f7 = mem:Rec@rec f7 F64
@field_ref rec_f8 = mem:Rec@rec f8 mem:Obj@ref
@field_ref obj_x = mem:Obj@ref x I64
@field_ref obj_y = mem:Obj@ref y I32
@field_ref obj_r0 = mem:Obj@ref r0 mem:Obj@ref
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
  @field f1 I32
    @flags PUBLIC
  @end
  @field f2 I16
    @flags PUBLIC
  @end
  @field f3 I8
    @flags PUBLIC
  @end
  @field f4 U8
    @flags PUBLIC
  @end
  @field f5 U16
    @flags PUBLIC
  @end
  @field f6 F32
    @flags PUBLIC
  @end
  @field f7 F64
    @flags PUBLIC
  @end
  @field f8 mem:Obj@ref
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field x I64
    @flags PUBLIC
  @end
  @field y I32
    @flags PUBLIC
  @end
  @field r0 mem:Obj@ref
    @flags PUBLIC
  @end
@end

@type default
  @method test_rec_64(I64, I64)I64
    @code
      @live.rec IR1
      @live.prim IR2
      st.field IR2, IR1, #rec_f0
      ld.field IR3, IR1, #rec_f0
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end
@end
