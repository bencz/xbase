// Memo files another program left in a state xbase does not write: a header whose first free block is below the end
// of the file (a new memo goes after the last one, never over it; the reference loses it), a record whose memo block
// is past the end (an empty memo; PACK makes it none), memos out of record order and two records on one memo (PACK
// gives each its own copy); a .dbt shorter than its header is an Open error (the reference opens it), a dBASE IV
// table (version 0x8B) with memos Corruption detected. The memo file is made ".dbt" also beside a table named in upper
// case, and found as ".DBT" there when there is no ".dbt". Functions that give back their argument unchanged keep a
// memo's "M" (AllTrim, PadR to its length), HardCR() makes a plain string. The files start with "rdd_memo_damage"
// (upper case for one), deleted at the end.
FIELD NAME, NOTE

PROCEDURE Main()
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, "[" + e:filename + "]", ;
      e:osCode ), Break( e ) } )
   Make()
   PutBytes( "rdd_memo_damage.dbt", 0, Chr( 0 ) + Chr( 0 ) + Chr( 0 ) + Chr( 0 ) )
   USE rdd_memo_damage NEW
   APPEND BLANK
   REPLACE NAME WITH "new", NOTE WITH "delta"
   Show( "low header:" )
   CLOSE
   Make()
   USE rdd_memo_damage NEW
   GO 2
   REPLACE NOTE WITH Replicate( "z", 600 ) // record 2's memo is now the last one, after record 3's
   CLOSE
   PutBytes( "rdd_memo_damage.dbf", Header0() + 5, "     99999" ) // record 1's memo: block 99999
   PutBytes( "rdd_memo_damage.dbf", Header0() + 2 * 15 + 5, ; // record 3 on record 2's memo
      GetBytes( "rdd_memo_damage.dbf", Header0() + 15 + 5, 10 ) )
   USE rdd_memo_damage NEW
   Show( "before pack:" )
   PACK
   Show( "packed:" )
   CLOSE
   ? "dbt size:", FSize( "rdd_memo_damage.dbt" )
   Make()
   Short( "rdd_memo_damage.dbt" )
   BEGIN SEQUENCE
      USE rdd_memo_damage NEW
   END SEQUENCE
   ? "short dbt:", Used()
   CLOSE ALL
   Make()
   PutBytes( "rdd_memo_damage.dbf", 0, Chr( 0x8B ) )
   BEGIN SEQUENCE
      USE rdd_memo_damage NEW
   END SEQUENCE
   ? "dbase iv:", Used()
   CLOSE ALL
   dbCreate( "RDD_MEMO_DAMAGE_UP.DBF", { { "NOTE", "M", 10, 0 } } )
   ? "upper case:", File( "RDD_MEMO_DAMAGE_UP.dbt" )
   USE RDD_MEMO_DAMAGE_UP.DBF NEW
   APPEND BLANK
   REPLACE NOTE WITH "x"
   CLOSE
   FRename( "RDD_MEMO_DAMAGE_UP.dbt", "RDD_MEMO_DAMAGE_UP.DBT" )
   USE RDD_MEMO_DAMAGE_UP.DBF NEW
   ? "kept:", ValType( AllTrim( NOTE ) ), ValType( PadR( NOTE, 1 ) ), ValType( HardCR( NOTE ) ), ;
      ValType( NOTE + "" ), ValType( PadR( NOTE, 2 ) )
   CLOSE
   FErase( "RDD_MEMO_DAMAGE_UP.DBF" )
   FErase( "RDD_MEMO_DAMAGE_UP.DBT" )
   FErase( "RDD_MEMO_DAMAGE_UP.dbt" )
   FErase( "rdd_memo_damage.dbf" )
   FErase( "rdd_memo_damage.dbt" )
RETURN

// Three records: "a" with "alpha" (block 1), "b" with "beta", "c" with "gamma".
STATIC PROCEDURE Make()
   dbCreate( "rdd_memo_damage", { { "NAME", "C", 4, 0 }, { "NOTE", "M", 10, 0 } } )
   USE rdd_memo_damage NEW
   APPEND BLANK
   REPLACE NAME WITH "a", NOTE WITH "alpha"
   APPEND BLANK
   REPLACE NAME WITH "b", NOTE WITH "beta"
   APPEND BLANK
   REPLACE NAME WITH "c", NOTE WITH "gamma"
   CLOSE
RETURN

STATIC PROCEDURE Show( cTitle )
   ? cTitle
   GO TOP
   DO WHILE ! Eof()
      ?? " " + Trim( NAME ) + "=" + Left( NOTE, 5 ) + "/" + LTrim( Str( Len( NOTE ) ) )
      SKIP
   ENDDO
RETURN

STATIC FUNCTION GetBytes( cFile, nAt, nLen )
   LOCAL h := FOpen( cFile ), c := Space( nLen )
   FSeek( h, nAt )
   FRead( h, @c, nLen )
   FClose( h )
RETURN c

// The table's header length: its records start there, 15 bytes each, the memo field 5 bytes in.
STATIC FUNCTION Header0()
RETURN 98

STATIC PROCEDURE PutBytes( cFile, nAt, cBytes )
   LOCAL h := FOpen( cFile, 2 )
   FSeek( h, nAt )
   FWrite( h, cBytes )
   FClose( h )
RETURN

// The file made two bytes long.
STATIC PROCEDURE Short( cFile )
   LOCAL h := FCreate( cFile )
   FWrite( h, Chr( 1 ) + Chr( 0 ) )
   FClose( h )
RETURN

STATIC FUNCTION FSize( cFile )
   LOCAL a := Directory( cFile )
RETURN IIf( Empty( a ), -1, a[ 1 ][ 2 ] )
