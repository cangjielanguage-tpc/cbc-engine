@type default {
  @method default main [ ] I64 {
    live.prim [ IR1 ]
    convert I16, I32, IR1, IR1
    ret.64 IR1
  }
}
