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
  @method test_lti_rec()I64
    @code
      load.type.info IR2, mem:Rec@rec
      ret.64 IR2
    @end
  @end

  @method test_lti_i64()I64
    @code
      load.type.info IR2, I64
      ret.64 IR2
    @end
  @end

  @method test_lti_u8()I64
    @code
      load.type.info IR2, U8
      ret.64 IR2
    @end
  @end

  @method test_lti_f64()I64
    @code
      load.type.info IR2, F64
      ret.64 IR2
    @end
  @end

  @method test_lti_bool()I64
    @code
      load.type.info IR2, Bool
      ret.64 IR2
    @end
  @end

  @method test_lti_arr()I64
    @code
      load.type.info IR2, @arr[mem:Rec@rec]
      ret.64 IR2
    @end
  @end
@end
