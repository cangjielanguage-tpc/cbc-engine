@type default {
  @method default test_I16_I32 [ ] I64 {
    live.prim [ IR1 ]
    convert I16, I32, IR1, IR1
    ret.64 IR1
  }

  @method default test_I16_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert I16, U32, IR1, IR1
    ret.64 IR1
  }

  @method default test_I32_F32 [ ] I64 {
    convert I32, F32, IR1, FR0
    ret.64 IR1
  }

  @method default test_I32_F64 [ ] I64 {
    convert I32, F64, IR1, FR0
    ret.64 IR1
  }

  @method default test_I32_I64 [ ] I64 {
    live.prim [ IR1 ]
    convert I32, I64, IR1, IR1
    ret.64 IR1
  }

  @method default test_I32_U64 [ ] I64 {
    live.prim [ IR1 ]
    convert I32, U64, IR1, IR1
    ret.64 IR1
  }

  @method default test_I64_F32 [ ] I64 {
    convert I64, F32, IR1, FR0
    ret.64 IR1
  }

  @method default test_I64_F64 [ ] I64 {
    convert I64, F64, IR1, FR0
    ret.64 IR1
  }

  @method default test_I64_I32 [ ] I64 {
    live.prim [ IR1 ]
    convert I64, I32, IR1, IR1
    ret.64 IR1
  }

  @method default test_I64_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert I64, U32, IR1, IR1
    ret.64 IR1
  }

  @method default test_I8_I32 [ ] I64 {
    live.prim [ IR1 ]
    convert I8, I32, IR1, IR1
    ret.64 IR1
  }

  @method default test_I8_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert I8, U32, IR1, IR1
    ret.64 IR1
  }

  @method default test_U16_I32 [ ] I64 {
    live.prim [ IR1 ]
    convert U16, I32, IR1, IR1
    ret.64 IR1
  }

  @method default test_U16_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert U16, U32, IR1, IR1
    ret.64 IR1
  }

  @method default test_U32_F32 [ ] I64 {
    convert U32, F32, IR1, FR0
    ret.64 IR1
  }

  @method default test_U32_F64 [ ] I64 {
    convert U32, F64, IR1, FR0
    ret.64 IR1
  }

  @method default test_U32_U64 [ ] I64 {
    live.prim [ IR1 ]
    convert U32, U64, IR1, IR1
    ret.64 IR1
  }

  @method default test_U64_F32 [ ] I64 {
    convert U64, F32, IR1, FR0
    ret.64 IR1
  }

  @method default test_U64_F64 [ ] I64 {
    convert U64, F64, IR1, FR0
    ret.64 IR1
  }

  @method default test_U8_I32 [ ] I64 {
    live.prim [ IR1 ]
    convert U8, I32, IR1, IR1
    ret.64 IR1
  }

  @method default test_U8_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert U8, U32, IR1, IR1
    ret.64 IR1
  }
}
