@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    remi.64 IR1, IR1, 0x1020
    ret.64 IR1
  }
}
