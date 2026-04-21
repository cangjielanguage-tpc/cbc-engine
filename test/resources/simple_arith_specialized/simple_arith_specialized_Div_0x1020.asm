@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    divi.64 IR1, IR1, 0x1020
    ret.64 IR1
  }
}
