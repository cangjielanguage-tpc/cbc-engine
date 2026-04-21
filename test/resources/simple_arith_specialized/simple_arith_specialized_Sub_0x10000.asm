@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    subi.64 IR1, IR1, 0x10000
    ret.64 IR1
  }
}
