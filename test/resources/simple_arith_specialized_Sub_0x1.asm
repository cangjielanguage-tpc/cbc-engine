@type default {
  @method default main [ ] I64 {
    subi.64 IR1, IR1, 0x1
    ret.64 IR1
  }
}
