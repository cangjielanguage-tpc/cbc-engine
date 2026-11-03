@main_type "default"

@method_ref default.fsum8 = default@ref fsum8(F64, F64, F64, F64, F64, F64, F64, F64)F64

@type default
  @method fsum8(F64, F64, F64, F64, F64, F64, F64, F64)F64
    @code
      fadd.64 FR0, FR0, FR1
      fadd.64 FR0, FR0, FR2
      fadd.64 FR0, FR0, FR3
      fadd.64 FR0, FR0, FR4
      fadd.64 FR0, FR0, FR5
      fadd.64 FR0, FR0, FR6
      fadd.64 FR0, FR0, FR7
      fret.64 FR0
    @end
  @end

  @method main()F64
    @code
      fmovi.64 FR0, 1.0
      fmovi.64 FR1, 2.0
      fmovi.64 FR2, 3.0
      fmovi.64 FR3, 4.0
      fmovi.64 FR4, 5.0
      fmovi.64 FR5, 6.0
      fmovi.64 FR6, 7.0
      fmovi.64 FR7, 8.0
      call.direct #default.fsum8, FR0, FR1, FR2, FR3, FR4, FR5, FR6, FR7
      fret.64 FR0
    @end
  @end
@end
