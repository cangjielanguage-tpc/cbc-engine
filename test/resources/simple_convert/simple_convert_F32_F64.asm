@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    convert F32, F64, FR0, FR0
    fret.64 FR0
  }
}
