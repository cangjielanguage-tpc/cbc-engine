@type default {
  @method default main [ ] I64 {
    fadd.64 FR0, FR0, FR1
    fret.64 FR0
  }
}
