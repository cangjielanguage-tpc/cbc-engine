@type default {
  @method default main [ ] I64 {
    xori.64 IR1, IR1, 0x100200
    ret.64 IR1
  }
}
