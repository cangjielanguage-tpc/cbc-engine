@type default {
  @method default main [ ] I64 {
    xori.64 IR1, IR1, 0xffffff
    xori.64 IR1, IR1, 0xff
    xori.64 IR1, IR1, 0xff0000
    xori.64 IR1, IR1, 0x7000000000000000
    ret.64 IR1
  }
}
