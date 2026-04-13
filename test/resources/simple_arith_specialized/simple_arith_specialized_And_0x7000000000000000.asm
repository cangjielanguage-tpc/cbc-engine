@type default {
  @method default main [ ] I64 {
    andi.64 IR1, IR1, 0x7000000000000000
    ret.64 IR1
  }
}
