@type default {
  @method default main [ ] I64 {
    ori.64 IR1, IR1, 0x10000
    ret.64 IR1
  }
}
