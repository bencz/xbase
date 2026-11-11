// PadL/PadR/PadC/Pad, Mod, Descend, the binary conversions, LenNum, SoundEx, the time sample functions and
// MemVarBlock, compared with the reference. A deliberate difference: an empty fill pads with blanks (the reference
// pads with zero bytes).
MEMVAR mv

PROCEDURE Main()
   LOCAL b
   PRIVATE mv := 5
   ? Box( PadR( "abc", 6 ) ), Box( PadL( "abc", 6, "*" ) ), Box( PadC( "abc", 8, "-" ) ), Box( PadC( "ab", 5 ) )
   ? Box( PadR( "abcdef", 3 ) ), Box( PadL( "abcdef", 3 ) ), Box( PadC( "abcdef", 3 ) )
   ? Box( PadR( 123, 6 ) ), Box( PadL( 12.5, 8 ) ), Box( PadR( Date() - Date(), 4 ) ), Box( PadR( .T., 3 ) ), ;
      Box( PadR( NIL, 3 ) )
   ? Box( PadR( "abc", -1 ) ), Box( PadR( "abc", 0 ) ), Box( PadL( "abc", 5, "xy" ) ), Box( PadR( "abc", 5, "" ) )
   ? Box( PadR( CToD( "" ), 10 ) ), Box( PadR( {}, 3 ) ), Box( Pad( "ab", 4 ) ), Box( PadR( -3, 5 ) ), ;
      Box( PadL( 1 / 3, 12 ) )
   ? Box( PadR( "abc", "5" ) ), Box( PadR( "abc" ) ), Box( PadR( "abc", 4.9 ) ), Box( PadL( "abc", 4, 65 ) )

   ? Mod( 7, 3 ), Mod( -7, 3 ), Mod( 7, -3 ), Mod( -7, -3 ), Mod( 7.5, 2 ), Mod( 0, 3 ), Mod( 5, 2.5 )
   ? Asc( Descend( "a" ) ), Asc( Descend( "z" ) ), Len( Descend( "abc" ) ), Descend( 5 ), Descend( -2.5 ), ;
      Descend( .T. ), Descend( CToD( "01/01/2000" ) ), Len( Descend( "" ) ), Asc( Descend( Chr( 0 ) ) ), ;
      Descend( NIL ), Descend( {} )

   ? Bin2I( Chr( 1 ) + Chr( 2 ) ), Bin2I( Chr( 255 ) + Chr( 255 ) ), Bin2W( Chr( 255 ) + Chr( 255 ) ), ;
      Bin2L( Chr( 1 ) + Chr( 0 ) + Chr( 0 ) + Chr( 128 ) ), Bin2I( "" ), Bin2I( "A" ), Bin2L( "AB" ), Bin2I( 5 )
   ? Asc( I2Bin( 258 ) ), Asc( SubStr( I2Bin( 258 ), 2 ) ), Len( I2Bin( 70000 ) ), Bin2I( I2Bin( -5 ) ), ;
      Bin2L( L2Bin( -123456 ) ), Len( L2Bin( 1 ) ), Bin2I( I2Bin( 1.9 ) ), Bin2W( I2Bin( -1 ) ), Len( I2Bin( "x" ) )
   ? LenNum( 123 ), LenNum( -1.5 ), LenNum( 0 ), LenNum( 1 / 3 ), LenNum( "abc" )

   ? SoundEx( "Robert" ), SoundEx( "Rupert" ), SoundEx( "Tymczak" ), SoundEx( "Pfister" ), SoundEx( "" ), ;
      SoundEx( "A" ), SoundEx( "123" ), SoundEx( "lloyd" ), SoundEx( "Ashcraft" ), SoundEx( "a b" )

   ? AMPM( "00:30:00" ), AMPM( "12:15:10" ), AMPM( "13:00:00" ), AMPM( "23:59:59" ), AMPM( "9:00" ), AMPM( "" )
   ? Days( 0 ), Days( 86399 ), Days( 86400 ), Days( 200000 ), Secs( "01:02:03" ), Secs( "25:00:00" ), Secs( "" )
   ? ElapTime( "10:00:00", "11:30:15" ), ElapTime( "23:00:00", "01:00:00" ), TString( 3723 ), TString( 90000 ), ;
      TString( 0 ), TString( 59.9 )

   b := MemVarBlock( "mv" )
   ? ValType( b ), Eval( b ), Eval( b, 9 ), mv, MemVarBlock( "nonexistent" ), ValType( MemVarBlock( 1 ) )
   b := MemVarBlock( "MV" )
   ? Eval( b, NIL ), mv, ValType( MemVarBlock( "mV" ) )

   ErrorBlock( {| e | QOut( "error:", e:subCode, e:operation, e:canSubstitute ), 99 } )
   ? Mod( "a", 2 ), Mod( 7, 0 )
RETURN

STATIC FUNCTION Box( c )
RETURN "[" + c + "]"
