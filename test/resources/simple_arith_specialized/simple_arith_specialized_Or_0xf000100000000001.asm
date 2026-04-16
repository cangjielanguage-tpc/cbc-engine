@type default {
  @method default main [ ] I64 {
    ori.64 IR1, IR1, 0xf000100000000001
    ret.64 IR1
  }
}
