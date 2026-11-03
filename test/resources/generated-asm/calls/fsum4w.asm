@main_type "default"

@method_ref default.fsumw4 = default@ref fsumw4(F32, F32, F32, F32)F32

@type default
  @method fsumw4(F32, F32, F32, F32)F32
    @code
      fadd.32 FR4, FR0, FR0
      fadd.32 FR4, FR4, FR4
      fadd.32 FR4, FR4, FR4
      fadd.32 FR5, FR1, FR1
      fadd.32 FR5, FR5, FR5
      fadd.32 FR4, FR4, FR5
      fadd.32 FR5, FR2, FR2
      fadd.32 FR4, FR4, FR5
      fadd.32 FR4, FR4, FR3
      fret.32 FR4
    @end
  @end

  @method main()F32
    @code
      fmovi.32 FR0, 1.5
      fmovi.32 FR1, 2.5
      fmovi.32 FR2, 3.5
      fmovi.32 FR3, 4.5
      call.direct #default.fsumw4, FR1, FR0, FR3, FR2
      fret.32 FR0
    @end
  @end
@end
