@type default {
  @method default test_F32_F64 [ ] I64 {
    live.prim [ IR1 ]
    convert F32, F64, FR0, FR0
    fret.64 FR0
  }

  @method default test_F32_I32 [ ] I64 {
    live.prim [ IR1 ]
    convert F32, I32, FR0, IR1
    fret.64 FR0
  }

  @method default test_F32_I64 [ ] I64 {
    live.prim [ IR1 ]
    convert F32, I64, FR0, IR1
    fret.64 FR0
  }

  @method default test_F32_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert F32, U32, FR0, IR1
    fret.64 FR0
  }

  @method default test_F32_U64 [ ] I64 {
    live.prim [ IR1 ]
    convert F32, U64, FR0, IR1
    fret.64 FR0
  }

  @method default test_F64_F32 [ ] I64 {
    live.prim [ IR1 ]
    convert F64, F32, FR0, FR0
    fret.64 FR0
  }

  @method default test_F64_I32 [ ] I64 {
    live.prim [ IR1 ]
    convert F64, I32, FR0, IR1
    fret.64 FR0
  }

  @method default test_F64_I64 [ ] I64 {
    live.prim [ IR1 ]
    convert F64, I64, FR0, IR1
    fret.64 FR0
  }

  @method default test_F64_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert F64, U32, FR0, IR1
    fret.64 FR0
  }

  @method default test_F64_U32 [ ] I64 {
    live.prim [ IR1 ]
    convert F64, U32, FR0, IR1
    fret.64 FR0
  }

  @method default test_F64_U64 [ ] I64 {
    live.prim [ IR1 ]
    convert F64, U64, FR0, IR1
    fret.64 FR0
  }
}
