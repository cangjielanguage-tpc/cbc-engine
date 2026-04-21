@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    uremi.64 IR1, IR1, 0x7000000000000000
    ret.64 IR1
  }
}
