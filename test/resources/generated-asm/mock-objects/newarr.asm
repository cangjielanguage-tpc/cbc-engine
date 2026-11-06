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
  @method test_len()I64
    @code
      @live.prim IR2
      newarr @arr[mem:Rec@rec]
      arr.len IR3, IR1
      @dead IR1
      ret.64 IR3
    @end
  @end

  @method test_len_prim()I64
    @code
      @live.prim IR2
      newarr @arr[I64]
      arr.len IR3, IR1
      @dead IR1
      ret.64 IR3
    @end
  @end

  @method test_len_g()I64
    @code
      @live.prim IR2
      load.type.info IR4, @arr[I64]
      newarr.g IR4, @arr[I64]
      @dead IR4
      arr.len IR3, IR1
      @dead IR1
      ret.64 IR3
    @end
  @end

  @method test_ptr()I64
    @code
      @live.prim IR2
      newarr @arr[mem:Rec@rec]
      ret.ref IR1
    @end
  @end

  @method test_ptr_prim()I64
    @code
      @live.prim IR2
      newarr @arr[I64]
      ret.ref IR1
    @end
  @end

  @method test_arr_ti()I64
    @code
      @live.prim IR2
      newarr @arr[mem:Rec@rec]
      @dead IR2
      load.type.info IR2, @arr[mem:Rec@rec]
      ld.raw.64 IR3, IR1, 0
      scc.64 EQ, IR4, IR2, IR3
      @dead IR1, IR2, IR3
      ret.64 IR4
    @end
  @end
@end
