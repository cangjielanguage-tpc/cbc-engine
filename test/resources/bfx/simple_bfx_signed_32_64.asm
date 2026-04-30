@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    bfxs.32.64 IR1, IR1, 0, 32
    ret.64 IR1
  }
}
