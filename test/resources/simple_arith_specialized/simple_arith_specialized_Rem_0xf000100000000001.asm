@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    remi.64 IR1, IR1, 0xf000100000000001
    ret.64 IR1
  }
}
