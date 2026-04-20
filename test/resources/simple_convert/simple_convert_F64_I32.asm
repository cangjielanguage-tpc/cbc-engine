@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    convert F64, I32, FR0, IR1
    fret.64 FR0
  }
}
