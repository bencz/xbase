// `{ k => v } $ h`: whether h has the key with the same value: equal numbers of either kind, strings byte for byte
// (SET EXACT aside), the same array; another value, a missing key, a key of another case, and a hash of no entry or
// of two (an argument error). `==` of an array with an object: .F., either way round.
PROCEDURE Main()
   LOCAL a := { 1 }, h := { "n" => 1, "s" => "ab", "a" => a, "d" => SToD( "20240229" ), "x" => NIL, 2 => .T. }, e
   ErrorBlock( {| x | Break( x ) } )
   SET EXACT ON
   ? { "n" => 1 } $ h, { "n" => 1.0 } $ h, { "n" => 2 } $ h, { "n" => "1" } $ h, { "s" => "ab" } $ h
   ? { "s" => "ab " } $ h, { "s" => "a" } $ h, { "a" => a } $ h, { "a" => { 1 } } $ h, { "x" => NIL } $ h
   ? { "d" => SToD( "20240229" ) } $ h, { "S" => "ab" } $ h, { 2 => .T. } $ h, { 2.0 => .T. } $ h, { 2 => .F. } $ h
   SET EXACT OFF
   ? { "s" => "a" } $ h, "n" $ h, "N" $ h, 2 $ h
   FOR EACH e IN { {|| { => } $ h }, {|| { "n" => 1, "s" => "ab" } $ h }, {|| h $ "x" } }
      BEGIN SEQUENCE
         ? Eval( e )
      RECOVER USING e
         ? "error", e:subCode, e:operation
      END SEQUENCE
   NEXT
   ? a == ErrorNew(), ErrorNew() == a, a == a, { 1 } == { 1 }
RETURN
