@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    xori.64 IR1, IR1, 0x1
    ret.64 IR1
  }
}
