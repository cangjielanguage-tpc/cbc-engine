@main_type "default"
@field_ref obj_x = mem:Obj@ref x I64
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field x I64
    @flags PUBLIC
  @end
@end

@type default
  @method test_box_ptr(I64)I64
    @code
      @live.prim IR1
      load.type.info IR3, mem:Obj@ref
      box.g IR1, IR2, IR3
      @dead IR1, IR3
      ret.ref IR2
    @end
  @end

  @method test_box_u8(I64)I64
    @code
      @live.prim IR1
      load.type.info IR3, mem:Obj@ref
      box.g IR1, IR2, IR3
      @dead IR1, IR3
      unbox IR4, IR2, U8
      @dead IR2
      ret.64 IR4
    @end
  @end

  @method test_box_i8(I64)I64
    @code
      @live.prim IR1
      load.type.info IR3, mem:Obj@ref
      box.g IR1, IR2, IR3
      @dead IR1, IR3
      unbox IR4, IR2, I8
      @dead IR2
      ret.64 IR4
    @end
  @end

  @method test_box_i64(I64)I64
    @code
      @live.prim IR1
      load.type.info IR3, mem:Obj@ref
      box.g IR1, IR2, IR3
      @dead IR1, IR3
      unbox IR4, IR2, I64
      @dead IR2
      ret.64 IR4
    @end
  @end

  @method test_box_ref()I64
    @code
      newobj mem:Obj@ref
      load.type.info IR3, mem:Obj@ref
      box.g IR1, IR2, IR3
      ld.raw.64 IR6, IR2, 0x8
      unbox IR4, IR2, mem:Obj@ref
      @dead IR1, IR2, IR3, IR4, IR6
      @live.prim IR1, IR4, IR6
      scc.64 EQ, IR7, IR6, IR1
      scc.64 EQ, IR8, IR4, IR1
      mul.64 IR7, IR7, IR8
      @dead IR1, IR4, IR6, IR8
      ret.64 IR7
    @end
  @end
@end
