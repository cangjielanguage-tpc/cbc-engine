@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    convert U32, F64, IR1, FR0
    ret.64 IR1
  }
}
