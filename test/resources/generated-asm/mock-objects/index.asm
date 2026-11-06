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
  @method test_index(I64, I64, I64)I64
    @code
      @live.rec IR1, IR4
      @live.prim IR2, IR3
      index IR4, IR1, IR2, @arr[mem:Rec@rec]
      st.raw.64 IR3, IR4, 0
      ld.raw.64 IR5, IR4, 0
      @dead IR1, IR2, IR3
      ret.64 IR5
    @end
  @end

  @method test_index_prim(I64, I64, I64)I64
    @code
      @live.rec IR1, IR4
      @live.prim IR2, IR3
      index IR4, IR1, IR2, @arr[I64]
      st.raw.64 IR3, IR4, 0
      ld.raw.64 IR5, IR4, 0
      @dead IR1, IR2, IR3
      ret.64 IR5
    @end
  @end
@end
