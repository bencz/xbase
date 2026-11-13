// BEGIN SEQUENCE inside loops of every kind (none, FOR, DO WHILE, FOR EACH), with every way of leaving it (nothing,
// BREAK, BREAK with a value, an error the error block breaks with, BREAK from a codeblock and from a function it
// calls, an error whose block substitutes a value, an inner sequence that recovers, one that breaks again from
// RECOVER, BREAK from a loop inside the sequence) and every handler (none,
// RECOVER, RECOVER USING, ALWAYS, both): each case shows the points it passed (a before, b in, c after the action,
// R or U and the value recovered, A always, d after the sequence, e after the loop), which way it left; a BREAK
// that leaves the function shows as ! and its value.
PROCEDURE Main()
   ErrorBlock( {| e | Break( e ) } )
   ? "none nothing none", Escaped( {|| Case0() } )
   ? "none nothing recover", Escaped( {|| Case1() } )
   ? "none nothing using", Escaped( {|| Case2() } )
   ? "none nothing always", Escaped( {|| Case3() } )
   ? "none nothing both", Escaped( {|| Case4() } )
   ? "none break none", Escaped( {|| Case5() } )
   ? "none break recover", Escaped( {|| Case6() } )
   ? "none break using", Escaped( {|| Case7() } )
   ? "none break always", Escaped( {|| Case8() } )
   ? "none break both", Escaped( {|| Case9() } )
   ? "none breakval none", Escaped( {|| Case10() } )
   ? "none breakval recover", Escaped( {|| Case11() } )
   ? "none breakval using", Escaped( {|| Case12() } )
   ? "none breakval always", Escaped( {|| Case13() } )
   ? "none breakval both", Escaped( {|| Case14() } )
   ? "none error none", Escaped( {|| Case15() } )
   ? "none error recover", Escaped( {|| Case16() } )
   ? "none error using", Escaped( {|| Case17() } )
   ? "none error always", Escaped( {|| Case18() } )
   ? "none error both", Escaped( {|| Case19() } )
   ? "none block none", Escaped( {|| Case20() } )
   ? "none block recover", Escaped( {|| Case21() } )
   ? "none block using", Escaped( {|| Case22() } )
   ? "none block always", Escaped( {|| Case23() } )
   ? "none block both", Escaped( {|| Case24() } )
   ? "none inner none", Escaped( {|| Case25() } )
   ? "none inner recover", Escaped( {|| Case26() } )
   ? "none inner using", Escaped( {|| Case27() } )
   ? "none inner always", Escaped( {|| Case28() } )
   ? "none inner both", Escaped( {|| Case29() } )
   ? "none subst none", Escaped( {|| Case30() } )
   ? "none subst recover", Escaped( {|| Case31() } )
   ? "none subst using", Escaped( {|| Case32() } )
   ? "none subst always", Escaped( {|| Case33() } )
   ? "none subst both", Escaped( {|| Case34() } )
   ? "none nested none", Escaped( {|| Case35() } )
   ? "none nested recover", Escaped( {|| Case36() } )
   ? "none nested using", Escaped( {|| Case37() } )
   ? "none nested always", Escaped( {|| Case38() } )
   ? "none nested both", Escaped( {|| Case39() } )
   ? "none rebreak none", Escaped( {|| Case40() } )
   ? "none rebreak recover", Escaped( {|| Case41() } )
   ? "none rebreak using", Escaped( {|| Case42() } )
   ? "none rebreak always", Escaped( {|| Case43() } )
   ? "none rebreak both", Escaped( {|| Case44() } )
   ? "none inloop none", Escaped( {|| Case45() } )
   ? "none inloop recover", Escaped( {|| Case46() } )
   ? "none inloop using", Escaped( {|| Case47() } )
   ? "none inloop always", Escaped( {|| Case48() } )
   ? "none inloop both", Escaped( {|| Case49() } )
   ? "for nothing none", Escaped( {|| Case50() } )
   ? "for nothing recover", Escaped( {|| Case51() } )
   ? "for nothing using", Escaped( {|| Case52() } )
   ? "for nothing always", Escaped( {|| Case53() } )
   ? "for nothing both", Escaped( {|| Case54() } )
   ? "for break none", Escaped( {|| Case55() } )
   ? "for break recover", Escaped( {|| Case56() } )
   ? "for break using", Escaped( {|| Case57() } )
   ? "for break always", Escaped( {|| Case58() } )
   ? "for break both", Escaped( {|| Case59() } )
   ? "for breakval none", Escaped( {|| Case60() } )
   ? "for breakval recover", Escaped( {|| Case61() } )
   ? "for breakval using", Escaped( {|| Case62() } )
   ? "for breakval always", Escaped( {|| Case63() } )
   ? "for breakval both", Escaped( {|| Case64() } )
   ? "for error none", Escaped( {|| Case65() } )
   ? "for error recover", Escaped( {|| Case66() } )
   ? "for error using", Escaped( {|| Case67() } )
   ? "for error always", Escaped( {|| Case68() } )
   ? "for error both", Escaped( {|| Case69() } )
   ? "for block none", Escaped( {|| Case70() } )
   ? "for block recover", Escaped( {|| Case71() } )
   ? "for block using", Escaped( {|| Case72() } )
   ? "for block always", Escaped( {|| Case73() } )
   ? "for block both", Escaped( {|| Case74() } )
   ? "for inner none", Escaped( {|| Case75() } )
   ? "for inner recover", Escaped( {|| Case76() } )
   ? "for inner using", Escaped( {|| Case77() } )
   ? "for inner always", Escaped( {|| Case78() } )
   ? "for inner both", Escaped( {|| Case79() } )
   ? "for subst none", Escaped( {|| Case80() } )
   ? "for subst recover", Escaped( {|| Case81() } )
   ? "for subst using", Escaped( {|| Case82() } )
   ? "for subst always", Escaped( {|| Case83() } )
   ? "for subst both", Escaped( {|| Case84() } )
   ? "for nested none", Escaped( {|| Case85() } )
   ? "for nested recover", Escaped( {|| Case86() } )
   ? "for nested using", Escaped( {|| Case87() } )
   ? "for nested always", Escaped( {|| Case88() } )
   ? "for nested both", Escaped( {|| Case89() } )
   ? "for rebreak none", Escaped( {|| Case90() } )
   ? "for rebreak recover", Escaped( {|| Case91() } )
   ? "for rebreak using", Escaped( {|| Case92() } )
   ? "for rebreak always", Escaped( {|| Case93() } )
   ? "for rebreak both", Escaped( {|| Case94() } )
   ? "for inloop none", Escaped( {|| Case95() } )
   ? "for inloop recover", Escaped( {|| Case96() } )
   ? "for inloop using", Escaped( {|| Case97() } )
   ? "for inloop always", Escaped( {|| Case98() } )
   ? "for inloop both", Escaped( {|| Case99() } )
   ? "while nothing none", Escaped( {|| Case100() } )
   ? "while nothing recover", Escaped( {|| Case101() } )
   ? "while nothing using", Escaped( {|| Case102() } )
   ? "while nothing always", Escaped( {|| Case103() } )
   ? "while nothing both", Escaped( {|| Case104() } )
   ? "while break none", Escaped( {|| Case105() } )
   ? "while break recover", Escaped( {|| Case106() } )
   ? "while break using", Escaped( {|| Case107() } )
   ? "while break always", Escaped( {|| Case108() } )
   ? "while break both", Escaped( {|| Case109() } )
   ? "while breakval none", Escaped( {|| Case110() } )
   ? "while breakval recover", Escaped( {|| Case111() } )
   ? "while breakval using", Escaped( {|| Case112() } )
   ? "while breakval always", Escaped( {|| Case113() } )
   ? "while breakval both", Escaped( {|| Case114() } )
   ? "while error none", Escaped( {|| Case115() } )
   ? "while error recover", Escaped( {|| Case116() } )
   ? "while error using", Escaped( {|| Case117() } )
   ? "while error always", Escaped( {|| Case118() } )
   ? "while error both", Escaped( {|| Case119() } )
   ? "while block none", Escaped( {|| Case120() } )
   ? "while block recover", Escaped( {|| Case121() } )
   ? "while block using", Escaped( {|| Case122() } )
   ? "while block always", Escaped( {|| Case123() } )
   ? "while block both", Escaped( {|| Case124() } )
   ? "while inner none", Escaped( {|| Case125() } )
   ? "while inner recover", Escaped( {|| Case126() } )
   ? "while inner using", Escaped( {|| Case127() } )
   ? "while inner always", Escaped( {|| Case128() } )
   ? "while inner both", Escaped( {|| Case129() } )
   ? "while subst none", Escaped( {|| Case130() } )
   ? "while subst recover", Escaped( {|| Case131() } )
   ? "while subst using", Escaped( {|| Case132() } )
   ? "while subst always", Escaped( {|| Case133() } )
   ? "while subst both", Escaped( {|| Case134() } )
   ? "while nested none", Escaped( {|| Case135() } )
   ? "while nested recover", Escaped( {|| Case136() } )
   ? "while nested using", Escaped( {|| Case137() } )
   ? "while nested always", Escaped( {|| Case138() } )
   ? "while nested both", Escaped( {|| Case139() } )
   ? "while rebreak none", Escaped( {|| Case140() } )
   ? "while rebreak recover", Escaped( {|| Case141() } )
   ? "while rebreak using", Escaped( {|| Case142() } )
   ? "while rebreak always", Escaped( {|| Case143() } )
   ? "while rebreak both", Escaped( {|| Case144() } )
   ? "while inloop none", Escaped( {|| Case145() } )
   ? "while inloop recover", Escaped( {|| Case146() } )
   ? "while inloop using", Escaped( {|| Case147() } )
   ? "while inloop always", Escaped( {|| Case148() } )
   ? "while inloop both", Escaped( {|| Case149() } )
   ? "each nothing none", Escaped( {|| Case150() } )
   ? "each nothing recover", Escaped( {|| Case151() } )
   ? "each nothing using", Escaped( {|| Case152() } )
   ? "each nothing always", Escaped( {|| Case153() } )
   ? "each nothing both", Escaped( {|| Case154() } )
   ? "each break none", Escaped( {|| Case155() } )
   ? "each break recover", Escaped( {|| Case156() } )
   ? "each break using", Escaped( {|| Case157() } )
   ? "each break always", Escaped( {|| Case158() } )
   ? "each break both", Escaped( {|| Case159() } )
   ? "each breakval none", Escaped( {|| Case160() } )
   ? "each breakval recover", Escaped( {|| Case161() } )
   ? "each breakval using", Escaped( {|| Case162() } )
   ? "each breakval always", Escaped( {|| Case163() } )
   ? "each breakval both", Escaped( {|| Case164() } )
   ? "each error none", Escaped( {|| Case165() } )
   ? "each error recover", Escaped( {|| Case166() } )
   ? "each error using", Escaped( {|| Case167() } )
   ? "each error always", Escaped( {|| Case168() } )
   ? "each error both", Escaped( {|| Case169() } )
   ? "each block none", Escaped( {|| Case170() } )
   ? "each block recover", Escaped( {|| Case171() } )
   ? "each block using", Escaped( {|| Case172() } )
   ? "each block always", Escaped( {|| Case173() } )
   ? "each block both", Escaped( {|| Case174() } )
   ? "each inner none", Escaped( {|| Case175() } )
   ? "each inner recover", Escaped( {|| Case176() } )
   ? "each inner using", Escaped( {|| Case177() } )
   ? "each inner always", Escaped( {|| Case178() } )
   ? "each inner both", Escaped( {|| Case179() } )
   ? "each subst none", Escaped( {|| Case180() } )
   ? "each subst recover", Escaped( {|| Case181() } )
   ? "each subst using", Escaped( {|| Case182() } )
   ? "each subst always", Escaped( {|| Case183() } )
   ? "each subst both", Escaped( {|| Case184() } )
   ? "each nested none", Escaped( {|| Case185() } )
   ? "each nested recover", Escaped( {|| Case186() } )
   ? "each nested using", Escaped( {|| Case187() } )
   ? "each nested always", Escaped( {|| Case188() } )
   ? "each nested both", Escaped( {|| Case189() } )
   ? "each rebreak none", Escaped( {|| Case190() } )
   ? "each rebreak recover", Escaped( {|| Case191() } )
   ? "each rebreak using", Escaped( {|| Case192() } )
   ? "each rebreak always", Escaped( {|| Case193() } )
   ? "each rebreak both", Escaped( {|| Case194() } )
   ? "each inloop none", Escaped( {|| Case195() } )
   ? "each inloop recover", Escaped( {|| Case196() } )
   ? "each inloop using", Escaped( {|| Case197() } )
   ? "each inloop always", Escaped( {|| Case198() } )
   ? "each inloop both", Escaped( {|| Case199() } )
RETURN

STATIC FUNCTION Case0()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case1()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case2()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case3()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case4()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case5()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case6()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case7()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case8()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case9()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case10()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK "v"
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case11()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK "v"
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case12()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK "v"
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case13()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK "v"
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case14()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BREAK "v"
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case15()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      y := 1 + "a"
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case16()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      y := 1 + "a"
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case17()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      y := 1 + "a"
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case18()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      y := 1 + "a"
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case19()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      y := 1 + "a"
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case20()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Eval( {|| Break( "blk" ) } )
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case21()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Eval( {|| Break( "blk" ) } )
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case22()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Eval( {|| Break( "blk" ) } )
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case23()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Eval( {|| Break( "blk" ) } )
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case24()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Eval( {|| Break( "blk" ) } )
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case25()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Inner()
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case26()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Inner()
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case27()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Inner()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case28()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Inner()
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case29()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Inner()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case30()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Substituted()
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case31()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Substituted()
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case32()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Substituted()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case33()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Substituted()
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case34()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Substituted()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case35()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Nested()
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case36()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Nested()
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case37()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Nested()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case38()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Nested()
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case39()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Nested()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case40()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Rebreak()
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case41()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Rebreak()
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case42()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Rebreak()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case43()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Rebreak()
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case44()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      Rebreak()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case45()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BreakInLoop()
      t += "c"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case46()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BreakInLoop()
      t += "c"
   RECOVER
      t += "R"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case47()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BreakInLoop()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case48()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BreakInLoop()
      t += "c"
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case49()
   LOCAL t := "", i, n := 0, x, y
   t += "a"
   BEGIN SEQUENCE
      t += "b"
      BreakInLoop()
      t += "c"
   RECOVER USING y
      t += "U" + Text( y )
   ALWAYS
      t += "A"
   END SEQUENCE
   t += "d"
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case50()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case51()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case52()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case53()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case54()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case55()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case56()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case57()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case58()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case59()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case60()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case61()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case62()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case63()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case64()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case65()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case66()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case67()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case68()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case69()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case70()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case71()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case72()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case73()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case74()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case75()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case76()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case77()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case78()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case79()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case80()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case81()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case82()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case83()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case84()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case85()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case86()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case87()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case88()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case89()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case90()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case91()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case92()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case93()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case94()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case95()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case96()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case97()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case98()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case99()
   LOCAL t := "", i, n := 0, x, y
   FOR i := 1 TO 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case100()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case101()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case102()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case103()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case104()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case105()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case106()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case107()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case108()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case109()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case110()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case111()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case112()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case113()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case114()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case115()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case116()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case117()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case118()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case119()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case120()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case121()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case122()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case123()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case124()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case125()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case126()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case127()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case128()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case129()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case130()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case131()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case132()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case133()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case134()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case135()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case136()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case137()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case138()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case139()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case140()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case141()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case142()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case143()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case144()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case145()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case146()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case147()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case148()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case149()
   LOCAL t := "", i, n := 0, x, y
   DO WHILE ++n <= 2
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   ENDDO
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case150()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case151()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case152()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case153()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case154()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case155()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case156()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case157()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case158()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case159()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case160()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case161()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case162()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case163()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case164()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BREAK "v"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case165()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case166()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case167()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case168()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case169()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         y := 1 + "a"
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case170()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case171()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case172()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case173()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case174()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Eval( {|| Break( "blk" ) } )
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case175()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case176()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case177()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case178()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case179()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Inner()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case180()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case181()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case182()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case183()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case184()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Substituted()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case185()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case186()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case187()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case188()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case189()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Nested()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case190()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case191()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case192()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case193()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case194()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         Rebreak()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case195()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case196()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER
         t += "R"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case197()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case198()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

STATIC FUNCTION Case199()
   LOCAL t := "", i, n := 0, x, y
   FOR EACH x IN { 1, 2 }
      t += "a"
      BEGIN SEQUENCE
         t += "b"
         BreakInLoop()
         t += "c"
      RECOVER USING y
         t += "U" + Text( y )
      ALWAYS
         t += "A"
      END SEQUENCE
      t += "d"
   NEXT
   t += "e"
RETURN t + iif( i == NIL .AND. n == 0 .AND. x == NIL .AND. y == NIL, "", "" )

// An inner sequence that recovers what it breaks with: nothing reaches the outer one.
STATIC PROCEDURE Nested()
   BEGIN SEQUENCE
      Break( "in" )
   RECOVER
   END SEQUENCE
RETURN

// An inner sequence whose RECOVER breaks again, with what it recovered.
STATIC PROCEDURE Rebreak()
   LOCAL x
   BEGIN SEQUENCE
      Break( "first" )
   RECOVER USING x
      Break( x + " again" )
   END SEQUENCE
RETURN

// BREAK from the second pass of a loop.
STATIC PROCEDURE BreakInLoop()
   LOCAL i
   FOR i := 1 TO 3
      IF i == 2
         BREAK i
      ENDIF
   NEXT
RETURN

// What the case gives, or what a BREAK that leaves it carries.
STATIC FUNCTION Escaped( b )
   LOCAL x, r
   BEGIN SEQUENCE
      r := Eval( b )
   RECOVER USING x
      r := "!" + Text( x )
   END SEQUENCE
RETURN r

STATIC PROCEDURE Inner()
   Break( "inner" )
RETURN

// An error whose block gives a value in its place: the code goes on.
STATIC FUNCTION Substituted()
   LOCAL bOld := ErrorBlock( {| e | iif( e:canSubstitute, 7, Break( e ) ) } ), y
   y := 1 + "a"
   ErrorBlock( bOld )
RETURN y

STATIC FUNCTION Text( x )
   IF ValType( x ) == "O"
      RETURN "<" + LTrim( Str( x:subCode ) ) + ">"
   ENDIF
RETURN iif( x == NIL, "nil", iif( ValType( x ) == "N", LTrim( Str( x ) ), x ) )
