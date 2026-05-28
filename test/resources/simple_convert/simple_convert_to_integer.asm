;strict
@main_type "default"

@type default
  @method test_I16_I32()I64
    @code
      @live.prim IR1
      i2i I32, I16, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_I16_U32()I64
    @code
      @live.prim IR1
      i2i U32, I16, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_I32_F32()I64
    @code
      f2i F32, I32, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_I32_F64()I64
    @code
      f2i F64, I32, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_I32_I64()I64
    @code
      @live.prim IR1
      i2i I64, I32, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_I32_U64()I64
    @code
      @live.prim IR1
      i2i U64, I32, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_I64_F32()I64
    @code
      f2i F32, I64, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_I64_F64()I64
    @code
      f2i F64, I64, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_I64_I32()I64
    @code
      @live.prim IR1
      i2i I32, I64, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_I64_U32()I64
    @code
      @live.prim IR1
      i2i U32, I64, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_I8_I32()I64
    @code
      @live.prim IR1
      i2i I32, I8, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_I8_U32()I64
    @code
      @live.prim IR1
      i2i U32, I8, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_U16_I32()I64
    @code
      @live.prim IR1
      i2i I32, U16, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_U16_U32()I64
    @code
      @live.prim IR1
      i2i U32, U16, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_U32_F32()I64
    @code
      f2i F32, U32, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_U32_F64()I64
    @code
      f2i F64, U32, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_U32_U64()I64
    @code
      @live.prim IR1
      i2i U64, U32, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_U64_F32()I64
    @code
      f2i F32, U64, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_U64_F64()I64
    @code
      f2i F64, U64, FR0, IR1
      ret.64 IR1
    @end
  @end

  @method test_U8_I32()I64
    @code
      @live.prim IR1
      i2i I32, U8, IR1, IR1
      ret.64 IR1
    @end
  @end

  @method test_U8_U32()I64
    @code
      @live.prim IR1
      i2i U32, U8, IR1, IR1
      ret.64 IR1
    @end
  @end
@end
