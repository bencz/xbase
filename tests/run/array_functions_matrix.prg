// The array functions over every combination of a list of arrays (empty, one element, mixed types, ten) and of
// starts, counts and targets (NIL, negative, zero, inside, past the end, fractional), with arguments made at run
// time and in macros, and with literal arguments (fewer of them). Each result shows as text; an error as E and its
// code.
PROCEDURE Main()
   ErrorBlock( {| e | Break( e ) } )
   F0()
   F1()
   F2()
   F3()
   F4()
   F5()
   F6()
   F7()
RETURN

STATIC PROCEDURE F0()
   LOCAL xa, xp, xc
   ? "AFill literal"
   ? ""
   ?? Shown( {|| AFill( AClone( {} ), 'x', NIL, NIL ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', NIL, -1 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', NIL, 2 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', -1, NIL ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', -1, -1 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', -1, 2 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 0, NIL ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 0, -1 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 0, 2 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 2, NIL ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 2, -1 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 2, 2 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 11, NIL ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 11, -1 ) } )
   ?? Shown( {|| AFill( AClone( {} ), 'x', 11, 2 ) } )
   ? ""
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', NIL, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', NIL, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', NIL, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', -1, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', -1, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', -1, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 0, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 0, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 0, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 2, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 2, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 2, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 11, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 11, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1 } ), 'x', 11, 2 ) } )
   ? ""
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', NIL, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', NIL, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', NIL, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', -1, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', -1, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', -1, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 0, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 0, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 0, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 2, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 2, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 2, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 11, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 11, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3 } ), 'x', 11, 2 ) } )
   ? ""
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', NIL, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', NIL, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', NIL, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', -1, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', -1, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', -1, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 0, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 0, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 0, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 2, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 2, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 2, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 11, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 11, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 'x', 11, 2 ) } )
   ? ""
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', NIL, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', NIL, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', NIL, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', -1, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', -1, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', -1, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 0, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 0, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 0, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 2, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 2, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 2, 2 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 11, NIL ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 11, -1 ) } )
   ?? Shown( {|| AFill( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 'x', 11, 2 ) } )
   ? "AFill runtime"
   FOR EACH xa IN Arrays()
      ? ""
      FOR EACH xp IN { NIL, -1, 0, 1, 2, 5, 11, 2.9 }
         FOR EACH xc IN { NIL, -1, 0, 2, 99 }
            ?? Shown( {|| AFill( AClone( xa ), 'x', xp, xc ) } )
         NEXT
      NEXT
   NEXT
   ? "AFill macro"
   FOR EACH xa IN Texts()
      ? ""
      FOR EACH xp IN { "NIL", "-1", "0", "1", "2", "5", "11", "2.9" }
         FOR EACH xc IN { "NIL", "-1", "0", "2", "99" }
            ?? Shown( {|| &( "AFill( AClone( " + xa + " ), 'x', " + xp + ", " + xc + " )" ) } )
         NEXT
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F1()
   LOCAL xa, xp, xc, xq
   ? "ACopy literal"
   ? ""
   ?? Shown( {|| ACopyTo( {}, NIL, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, NIL, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, NIL, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, NIL, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, NIL, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, NIL, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, -1, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, -1, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, -1, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, -1, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, -1, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, -1, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 0, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 0, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 0, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 0, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 0, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 0, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 2, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 2, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 2, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 2, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 2, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 2, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 11, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 11, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 11, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 11, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( {}, 11, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( {}, 11, 2, 3 ) } )
   ? ""
   ?? Shown( {|| ACopyTo( { 1 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, NIL, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, NIL, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, NIL, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, NIL, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, NIL, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, -1, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, -1, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, -1, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, -1, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, -1, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, -1, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 0, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 0, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 0, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 0, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 0, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 0, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 2, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 2, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 2, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 2, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 2, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 2, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 11, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 11, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 11, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 11, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 11, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1 }, 11, 2, 3 ) } )
   ? ""
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, NIL, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, NIL, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, NIL, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, NIL, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, NIL, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, -1, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, -1, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, -1, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, -1, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, -1, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, -1, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 0, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 0, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 0, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 0, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 0, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 0, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 2, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 2, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 2, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 2, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 2, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 2, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 11, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 11, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 11, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 11, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 11, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3 }, 11, 2, 3 ) } )
   ? ""
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, -1, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, -1, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, -1, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, -1, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, -1, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, -1, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 0, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 0, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 0, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 0, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 0, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 0, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 2, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 2, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 2, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 2, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 2, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 2, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 11, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 11, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 11, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 11, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 11, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 'a', NIL, .T., { 1 }, 2.5 }, 11, 2, 3 ) } )
   ? ""
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, 2, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, NIL, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, NIL, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, -1, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, -1, 3 ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, 2, NIL ) } )
   ?? Shown( {|| ACopyTo( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, 2, 3 ) } )
   ? "ACopy runtime"
   FOR EACH xa IN Arrays()
      ? ""
      FOR EACH xp IN { NIL, -1, 0, 1, 2, 5, 11, 2.9 }
         FOR EACH xc IN { NIL, -1, 0, 2, 99 }
            FOR EACH xq IN { NIL, 0, 3, 5 }
               ?? Shown( {|| ACopyTo( xa, xp, xc, xq ) } )
            NEXT
         NEXT
      NEXT
   NEXT
   ? "ACopy macro"
   FOR EACH xa IN Texts()
      ? ""
      FOR EACH xp IN { "NIL", "-1", "0", "1", "2", "5", "11", "2.9" }
         FOR EACH xc IN { "NIL", "-1", "0", "2", "99" }
            FOR EACH xq IN { "NIL", "0", "3", "5" }
               ?? Shown( {|| &( "ACopyTo( " + xa + ", " + xp + ", " + xc + ", " + xq + " )" ) } )
            NEXT
         NEXT
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F2()
   LOCAL xa, xp
   ? "ADel literal"
   ? ""
   ?? Shown( {|| ADel( AClone( {} ), NIL ) } )
   ?? Shown( {|| ADel( AClone( {} ), -1 ) } )
   ?? Shown( {|| ADel( AClone( {} ), 0 ) } )
   ?? Shown( {|| ADel( AClone( {} ), 2 ) } )
   ?? Shown( {|| ADel( AClone( {} ), 11 ) } )
   ? ""
   ?? Shown( {|| ADel( AClone( { 1 } ), NIL ) } )
   ?? Shown( {|| ADel( AClone( { 1 } ), -1 ) } )
   ?? Shown( {|| ADel( AClone( { 1 } ), 0 ) } )
   ?? Shown( {|| ADel( AClone( { 1 } ), 2 ) } )
   ?? Shown( {|| ADel( AClone( { 1 } ), 11 ) } )
   ? ""
   ?? Shown( {|| ADel( AClone( { 1, 2, 3 } ), NIL ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3 } ), -1 ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3 } ), 0 ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3 } ), 2 ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3 } ), 11 ) } )
   ? ""
   ?? Shown( {|| ADel( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), NIL ) } )
   ?? Shown( {|| ADel( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), -1 ) } )
   ?? Shown( {|| ADel( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 0 ) } )
   ?? Shown( {|| ADel( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 2 ) } )
   ?? Shown( {|| ADel( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 11 ) } )
   ? ""
   ?? Shown( {|| ADel( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), NIL ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), -1 ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 0 ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 2 ) } )
   ?? Shown( {|| ADel( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 11 ) } )
   ? "ADel runtime"
   FOR EACH xa IN Arrays()
      ? ""
      FOR EACH xp IN { NIL, -1, 0, 1, 2, 5, 11, 2.9 }
         ?? Shown( {|| ADel( AClone( xa ), xp ) } )
      NEXT
   NEXT
   ? "ADel macro"
   FOR EACH xa IN Texts()
      ? ""
      FOR EACH xp IN { "NIL", "-1", "0", "1", "2", "5", "11", "2.9" }
         ?? Shown( {|| &( "ADel( AClone( " + xa + " ), " + xp + " )" ) } )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F3()
   LOCAL xa, xp
   ? "AIns literal"
   ? ""
   ?? Shown( {|| AIns( AClone( {} ), NIL ) } )
   ?? Shown( {|| AIns( AClone( {} ), -1 ) } )
   ?? Shown( {|| AIns( AClone( {} ), 0 ) } )
   ?? Shown( {|| AIns( AClone( {} ), 2 ) } )
   ?? Shown( {|| AIns( AClone( {} ), 11 ) } )
   ? ""
   ?? Shown( {|| AIns( AClone( { 1 } ), NIL ) } )
   ?? Shown( {|| AIns( AClone( { 1 } ), -1 ) } )
   ?? Shown( {|| AIns( AClone( { 1 } ), 0 ) } )
   ?? Shown( {|| AIns( AClone( { 1 } ), 2 ) } )
   ?? Shown( {|| AIns( AClone( { 1 } ), 11 ) } )
   ? ""
   ?? Shown( {|| AIns( AClone( { 1, 2, 3 } ), NIL ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3 } ), -1 ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3 } ), 0 ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3 } ), 2 ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3 } ), 11 ) } )
   ? ""
   ?? Shown( {|| AIns( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), NIL ) } )
   ?? Shown( {|| AIns( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), -1 ) } )
   ?? Shown( {|| AIns( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 0 ) } )
   ?? Shown( {|| AIns( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 2 ) } )
   ?? Shown( {|| AIns( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 11 ) } )
   ? ""
   ?? Shown( {|| AIns( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), NIL ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), -1 ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 0 ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 2 ) } )
   ?? Shown( {|| AIns( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 11 ) } )
   ? "AIns runtime"
   FOR EACH xa IN Arrays()
      ? ""
      FOR EACH xp IN { NIL, -1, 0, 1, 2, 5, 11, 2.9 }
         ?? Shown( {|| AIns( AClone( xa ), xp ) } )
      NEXT
   NEXT
   ? "AIns macro"
   FOR EACH xa IN Texts()
      ? ""
      FOR EACH xp IN { "NIL", "-1", "0", "1", "2", "5", "11", "2.9" }
         ?? Shown( {|| &( "AIns( AClone( " + xa + " ), " + xp + " )" ) } )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F4()
   LOCAL xa, xc
   ? "ASize literal"
   ? ""
   ?? Shown( {|| ASize( AClone( {} ), NIL ) } )
   ?? Shown( {|| ASize( AClone( {} ), -1 ) } )
   ?? Shown( {|| ASize( AClone( {} ), 2 ) } )
   ? ""
   ?? Shown( {|| ASize( AClone( { 1 } ), NIL ) } )
   ?? Shown( {|| ASize( AClone( { 1 } ), -1 ) } )
   ?? Shown( {|| ASize( AClone( { 1 } ), 2 ) } )
   ? ""
   ?? Shown( {|| ASize( AClone( { 1, 2, 3 } ), NIL ) } )
   ?? Shown( {|| ASize( AClone( { 1, 2, 3 } ), -1 ) } )
   ?? Shown( {|| ASize( AClone( { 1, 2, 3 } ), 2 ) } )
   ? ""
   ?? Shown( {|| ASize( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), NIL ) } )
   ?? Shown( {|| ASize( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), -1 ) } )
   ?? Shown( {|| ASize( AClone( { 'a', NIL, .T., { 1 }, 2.5 } ), 2 ) } )
   ? ""
   ?? Shown( {|| ASize( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), NIL ) } )
   ?? Shown( {|| ASize( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), -1 ) } )
   ?? Shown( {|| ASize( AClone( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ), 2 ) } )
   ? "ASize runtime"
   FOR EACH xa IN Arrays()
      ? ""
      FOR EACH xc IN { NIL, -1, 0, 2, 99 }
         ?? Shown( {|| ASize( AClone( xa ), xc ) } )
      NEXT
   NEXT
   ? "ASize macro"
   FOR EACH xa IN Texts()
      ? ""
      FOR EACH xc IN { "NIL", "-1", "0", "2", "99" }
         ?? Shown( {|| &( "ASize( AClone( " + xa + " ), " + xc + " )" ) } )
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F5()
   LOCAL xa, xx, xp, xc
   ? "AScan literal"
   ? ""
   ?? Shown( {|| AScan( {}, 3, NIL, NIL ) } )
   ?? Shown( {|| AScan( {}, 3, NIL, -1 ) } )
   ?? Shown( {|| AScan( {}, 3, NIL, 2 ) } )
   ?? Shown( {|| AScan( {}, 3, -1, NIL ) } )
   ?? Shown( {|| AScan( {}, 3, -1, -1 ) } )
   ?? Shown( {|| AScan( {}, 3, -1, 2 ) } )
   ?? Shown( {|| AScan( {}, 3, 0, NIL ) } )
   ?? Shown( {|| AScan( {}, 3, 0, -1 ) } )
   ?? Shown( {|| AScan( {}, 3, 0, 2 ) } )
   ?? Shown( {|| AScan( {}, 3, 2, NIL ) } )
   ?? Shown( {|| AScan( {}, 3, 2, -1 ) } )
   ?? Shown( {|| AScan( {}, 3, 2, 2 ) } )
   ?? Shown( {|| AScan( {}, 3, 11, NIL ) } )
   ?? Shown( {|| AScan( {}, 3, 11, -1 ) } )
   ?? Shown( {|| AScan( {}, 3, 11, 2 ) } )
   ?? Shown( {|| AScan( {}, NIL, NIL, NIL ) } )
   ?? Shown( {|| AScan( {}, NIL, NIL, -1 ) } )
   ?? Shown( {|| AScan( {}, NIL, NIL, 2 ) } )
   ?? Shown( {|| AScan( {}, NIL, -1, NIL ) } )
   ?? Shown( {|| AScan( {}, NIL, -1, -1 ) } )
   ?? Shown( {|| AScan( {}, NIL, -1, 2 ) } )
   ?? Shown( {|| AScan( {}, NIL, 0, NIL ) } )
   ?? Shown( {|| AScan( {}, NIL, 0, -1 ) } )
   ?? Shown( {|| AScan( {}, NIL, 0, 2 ) } )
   ?? Shown( {|| AScan( {}, NIL, 2, NIL ) } )
   ?? Shown( {|| AScan( {}, NIL, 2, -1 ) } )
   ?? Shown( {|| AScan( {}, NIL, 2, 2 ) } )
   ?? Shown( {|| AScan( {}, NIL, 11, NIL ) } )
   ?? Shown( {|| AScan( {}, NIL, 11, -1 ) } )
   ?? Shown( {|| AScan( {}, NIL, 11, 2 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, NIL, NIL ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, NIL, -1 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, NIL, 2 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, -1, NIL ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, -1, -1 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, -1, 2 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 0, NIL ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 0, -1 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 0, 2 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 2, NIL ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 2, -1 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 2, 2 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 11, NIL ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 11, -1 ) } )
   ?? Shown( {|| AScan( {}, {| v | v == 2 }, 11, 2 ) } )
   ? ""
   ?? Shown( {|| AScan( { 1 }, 3, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, 3, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, 3, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, 3, 11, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, NIL, 11, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1 }, {| v | v == 2 }, 11, 2 ) } )
   ? ""
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, 3, 11, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, NIL, 11, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3 }, {| v | v == 2 }, 11, 2 ) } )
   ? ""
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, -1, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, -1, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, -1, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 0, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 0, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 0, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 2, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 2, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 2, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 11, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 11, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, 3, 11, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, -1, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, -1, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, -1, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 0, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 0, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 0, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 2, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 2, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 2, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 11, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 11, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 11, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, -1, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, -1, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, -1, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 0, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 0, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 0, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 2, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 2, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 2, 2 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 11, NIL ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 11, -1 ) } )
   ?? Shown( {|| AScan( { 'a', NIL, .T., { 1 }, 2.5 }, {| v | v == 2 }, 11, 2 ) } )
   ? ""
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 3, 11, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 11, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, NIL, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, NIL, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, NIL, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, -1, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, -1, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, -1, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 0, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 0, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 0, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 2, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 2, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 2, 2 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 11, NIL ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 11, -1 ) } )
   ?? Shown( {|| AScan( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, {| v | v == 2 }, 11, 2 ) } )
   ? "AScan runtime"
   FOR EACH xa IN Arrays()
      ? ""
      FOR EACH xx IN { 3, 'a', NIL, {| v | v == 2 } }
         FOR EACH xp IN { NIL, -1, 0, 1, 2, 5, 11, 2.9 }
            FOR EACH xc IN { NIL, -1, 0, 2, 99 }
               ?? Shown( {|| AScan( xa, xx, xp, xc ) } )
            NEXT
         NEXT
      NEXT
   NEXT
   ? "AScan macro"
   FOR EACH xa IN Texts()
      ? ""
      FOR EACH xx IN { "3", "'a'", "NIL", "{| v | v == 2 }" }
         FOR EACH xp IN { "NIL", "-1", "0", "1", "2", "5", "11", "2.9" }
            FOR EACH xc IN { "NIL", "-1", "0", "2", "99" }
               ?? Shown( {|| &( "AScan( " + xa + ", " + xx + ", " + xp + ", " + xc + " )" ) } )
            NEXT
         NEXT
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F6()
   LOCAL xa, xp, xc
   ? "AEval literal"
   ? ""
   ?? Shown( {|| Visited( {}, NIL, NIL ) } )
   ?? Shown( {|| Visited( {}, NIL, -1 ) } )
   ?? Shown( {|| Visited( {}, NIL, 2 ) } )
   ?? Shown( {|| Visited( {}, -1, NIL ) } )
   ?? Shown( {|| Visited( {}, -1, -1 ) } )
   ?? Shown( {|| Visited( {}, -1, 2 ) } )
   ?? Shown( {|| Visited( {}, 0, NIL ) } )
   ?? Shown( {|| Visited( {}, 0, -1 ) } )
   ?? Shown( {|| Visited( {}, 0, 2 ) } )
   ?? Shown( {|| Visited( {}, 2, NIL ) } )
   ?? Shown( {|| Visited( {}, 2, -1 ) } )
   ?? Shown( {|| Visited( {}, 2, 2 ) } )
   ?? Shown( {|| Visited( {}, 11, NIL ) } )
   ?? Shown( {|| Visited( {}, 11, -1 ) } )
   ?? Shown( {|| Visited( {}, 11, 2 ) } )
   ? ""
   ?? Shown( {|| Visited( { 1 }, NIL, NIL ) } )
   ?? Shown( {|| Visited( { 1 }, NIL, -1 ) } )
   ?? Shown( {|| Visited( { 1 }, NIL, 2 ) } )
   ?? Shown( {|| Visited( { 1 }, -1, NIL ) } )
   ?? Shown( {|| Visited( { 1 }, -1, -1 ) } )
   ?? Shown( {|| Visited( { 1 }, -1, 2 ) } )
   ?? Shown( {|| Visited( { 1 }, 0, NIL ) } )
   ?? Shown( {|| Visited( { 1 }, 0, -1 ) } )
   ?? Shown( {|| Visited( { 1 }, 0, 2 ) } )
   ?? Shown( {|| Visited( { 1 }, 2, NIL ) } )
   ?? Shown( {|| Visited( { 1 }, 2, -1 ) } )
   ?? Shown( {|| Visited( { 1 }, 2, 2 ) } )
   ?? Shown( {|| Visited( { 1 }, 11, NIL ) } )
   ?? Shown( {|| Visited( { 1 }, 11, -1 ) } )
   ?? Shown( {|| Visited( { 1 }, 11, 2 ) } )
   ? ""
   ?? Shown( {|| Visited( { 1, 2, 3 }, NIL, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, NIL, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, NIL, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, -1, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, -1, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, -1, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 0, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 0, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 0, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 2, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 2, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 2, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 11, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 11, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3 }, 11, 2 ) } )
   ? ""
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, NIL ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, -1 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, NIL, 2 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, -1, NIL ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, -1, -1 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, -1, 2 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 0, NIL ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 0, -1 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 0, 2 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 2, NIL ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 2, -1 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 2, 2 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 11, NIL ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 11, -1 ) } )
   ?? Shown( {|| Visited( { 'a', NIL, .T., { 1 }, 2.5 }, 11, 2 ) } )
   ? ""
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, NIL, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, -1, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 0, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 2, 2 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, NIL ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, -1 ) } )
   ?? Shown( {|| Visited( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, 11, 2 ) } )
   ? "AEval runtime"
   FOR EACH xa IN Arrays()
      ? ""
      FOR EACH xp IN { NIL, -1, 0, 1, 2, 5, 11, 2.9 }
         FOR EACH xc IN { NIL, -1, 0, 2, 99 }
            ?? Shown( {|| Visited( xa, xp, xc ) } )
         NEXT
      NEXT
   NEXT
   ? "AEval macro"
   FOR EACH xa IN Texts()
      ? ""
      FOR EACH xp IN { "NIL", "-1", "0", "1", "2", "5", "11", "2.9" }
         FOR EACH xc IN { "NIL", "-1", "0", "2", "99" }
            ?? Shown( {|| &( "Visited( " + xa + ", " + xp + ", " + xc + " )" ) } )
         NEXT
      NEXT
   NEXT
RETURN

STATIC PROCEDURE F7()
   LOCAL xa
   ? "ATail literal"
   ? ""
   ?? Shown( {|| ATail( {} ) } )
   ? ""
   ?? Shown( {|| ATail( { 1 } ) } )
   ? ""
   ?? Shown( {|| ATail( { 1, 2, 3 } ) } )
   ? ""
   ?? Shown( {|| ATail( { 'a', NIL, .T., { 1 }, 2.5 } ) } )
   ? ""
   ?? Shown( {|| ATail( { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } ) } )
   ? "ATail runtime"
   FOR EACH xa IN Arrays()
      ? ""
      ?? Shown( {|| ATail( xa ) } )
   NEXT
   ? "ATail macro"
   FOR EACH xa IN Texts()
      ? ""
      ?? Shown( {|| &( "ATail( " + xa + " )" ) } )
   NEXT
RETURN

STATIC FUNCTION Arrays()
RETURN { {}, { 1 }, { 1, 2, 3 }, { 'a', NIL, .T., { 1 }, 2.5 }, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } }

STATIC FUNCTION Texts()
RETURN { "{}", "{ 1 }", "{ 1, 2, 3 }", "{ 'a', NIL, .T., { 1 }, 2.5 }", "{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }" }

// ACopy() of the array into a target of four "-", from the target position; shows the target (public: macros call it).
FUNCTION ACopyTo( a, nStart, nCount, nTarget )
   LOCAL t := { "-", "-", "-", "-" }
   ACopy( a, t, nStart, nCount, nTarget )
RETURN t

// The elements AEval() visits, with their positions.
FUNCTION Visited( a, nStart, nCount )
   LOCAL v := {}
   AEval( a, {| x, i | AAdd( v, { i, x } ) }, nStart, nCount )
RETURN v

// The value a block gives, as text, or E and the code of its error.
STATIC FUNCTION Shown( b )
   LOCAL x, e
   BEGIN SEQUENCE
      x := Eval( b )
   RECOVER USING e
      RETURN " E" + LTrim( Str( e:subCode ) )
   END SEQUENCE
RETURN " " + Text( x )

STATIC FUNCTION Text( x )
   LOCAL c, y
   SWITCH ValType( x )
   CASE "A"
      c := "{"
      FOR EACH y IN x
         c += Text( y ) + ";"
      NEXT
      RETURN c + "}"
   CASE "N"
      RETURN LTrim( Str( x ) )
   CASE "C"
      RETURN x
   CASE "L"
      RETURN iif( x, "T", "F" )
   CASE "B"
      RETURN "b"
   ENDSWITCH
RETURN "nil"
