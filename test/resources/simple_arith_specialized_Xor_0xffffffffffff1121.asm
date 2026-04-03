@type default {
  @method default main [ ] I64 {
    xori.64 IR1, IR1, 0xffffffffffff1121
    ret.64 IR1
  }
}
