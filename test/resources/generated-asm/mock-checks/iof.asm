@main_type "default"
@field_ref obj_r0 = mem:Obj@ref r0 mem:Obj@ref
@type mem:Rec
  @flags PUBLIC RECORD
  @field f0 I64
    @flags PUBLIC
  @end
@end

@type mem:Obj
  @field r0 mem:Obj@ref
    @flags PUBLIC
  @end
@end

@type default
  @method test_iof_self()I64
    @code
      newobj mem:Rec@rec
      iof IR2, IR1, mem:Rec@rec
      @dead IR1
      ret.64 IR2
    @end
  @end

  @method test_iof_other()I64
    @code
      newobj mem:Rec@rec
      iof IR2, IR1, mem:Obj@ref
      @dead IR1
      ret.64 IR2
    @end
  @end

  @method test_iof_null(I64)I64
    @code
      @live.ref IR1
      iof IR2, IR1, mem:Rec@rec
      @dead IR1
      ret.64 IR2
    @end
  @end

  @method test_iof_g_self()I64
    @code
      newobj mem:Rec@rec
      load.type.info IR3, mem:Rec@rec
      iof.g IR2, IR1, IR3
      @dead IR1, IR3
      ret.64 IR2
    @end
  @end

  @method test_iof_g_other()I64
    @code
      newobj mem:Rec@rec
      load.type.info IR3, mem:Obj@ref
      iof.g IR2, IR1, IR3
      @dead IR1, IR3
      ret.64 IR2
    @end
  @end

  @method test_iof_g_null(I64)I64
    @code
      @live.ref IR1
      load.type.info IR3, mem:Rec@rec
      iof.g IR2, IR1, IR3
      @dead IR1, IR3
      ret.64 IR2
    @end
  @end
@end
