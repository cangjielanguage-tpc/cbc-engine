;strict
@main_type "default"

@type default
  @method test_F32_F64()I64
    @code
      @live.prim IR1
      f2f F64, F32, FR0, FR0
      fret.64 FR0
    @end
  @end

  @method test_F32_I32()I64
    @code
      @live.prim IR1
      i2f I32, F32, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F32_I64()I64
    @code
      @live.prim IR1
      i2f I64, F32, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F32_U32()I64
    @code
      @live.prim IR1
      i2f U32, F32, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F32_U64()I64
    @code
      @live.prim IR1
      i2f U64, F32, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F64_F32()I64
    @code
      @live.prim IR1
      f2f F32, F64, FR0, FR0
      fret.64 FR0
    @end
  @end

  @method test_F64_I32()I64
    @code
      @live.prim IR1
      i2f I32, F64, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F64_I64()I64
    @code
      @live.prim IR1
      i2f I64, F64, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F64_U32()I64
    @code
      @live.prim IR1
      i2f U32, F64, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F64_U32()I64
    @code
      @live.prim IR1
      i2f U32, F64, IR1, FR0
      fret.64 FR0
    @end
  @end

  @method test_F64_U64()I64
    @code
      @live.prim IR1
      i2f U64, F64, IR1, FR0
      fret.64 FR0
    @end
  @end
@end
