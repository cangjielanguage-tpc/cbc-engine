@main_type "default"
@field_ref atm_f8s = atm:Cell@rec f8s I8
@field_ref atm_f8u = atm:Cell@rec f8u U8
@field_ref atm_f16s = atm:Cell@rec f16s I16
@field_ref atm_f16u = atm:Cell@rec f16u U16
@field_ref atm_f32 = atm:Cell@rec f32 I32
@field_ref atm_f64 = atm:Cell@rec f64 I64
@type atm:Cell
  @flags PUBLIC RECORD
  @field f8s I8
    @flags PUBLIC
  @end
  @field f8u U8
    @flags PUBLIC
  @end
  @field f16s I16
    @flags PUBLIC
  @end
  @field f16u U16
    @flags PUBLIC
  @end
  @field f32 I32
    @flags PUBLIC
  @end
  @field f64 I64
    @flags PUBLIC
  @end
@end

@type default
  @method test_st_32(I64, I64)I64
    @code
      @live.ref IR1
      @live.prim IR2
      atomic.store IR2, IR1, #atm_f32
      atomic.load IR3, IR1, #atm_f32
      @dead IR1, IR2
      ret.64 IR3
    @end
  @end
@end
