@type default {
  @method default main [ ] I64 {
    andi.64 IR1, IR1, 0xffffffffffff1121
    ret.64 IR1
  }
}
