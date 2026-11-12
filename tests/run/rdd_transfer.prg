// Copies between files: COPY TO and APPEND FROM between tables (every field: the records' bytes as they are; FIELDS,
// in their order, unknown names left out: the values; the scope, SET DELETED, a deleted record's copy deleted; a
// value of another type a Data type error the copy goes on after when defaulted; a memo into a character field and
// back; the FOR block of APPEND FROM reading the file's fields), to and from SDF and DELIMITED text files (WITH
// BLANK, TAB, a character, an array; quotes, separators inside them, missing and extra fields, line ends CR, LF, CR
// LF and LF CR, Ctrl-Z dropped), COPY STRUCTURE [EXTENDED] (a 300-byte character field), CREATE [FROM], a copy to a
// DBFCDX table, APPEND FROM the table itself, the errors. Deliberate differences: a field named twice in FIELDS is
// copied once; a line of a text file read into a table is read as the table stores values (a date that is not one
// is blank, a logical Y is T); DELIMITED WITH takes the clauses after it; APPEND FROM the table itself copies the
// records it had; an unknown driver is DBCMD/1015; error blocks that close the file being read and odd structures and
// delimiters do no harm. The files start with "rdd_transfer_", deleted at the end.
FIELD NAME, AMT, D, L, NOTE, EXTRA, FIELD_NAME, FIELD_TYPE, FIELD_LEN, FIELD_DEC
REQUEST DBFCDX, SDF, DELIM

#ifdef __PLATFORM__WINDOWS
#define EOL Chr( 13 ) + Chr( 10 )
#else
#define EOL Chr( 10 )
#endif

PROCEDURE Main()
   AEval( Directory( "rdd_transfer_*.*" ), {| a | FErase( a[ 1 ] ) } ) // left by a run that failed
   Tables1()
   Texts()
   Structures()
   Hostile()
   AEval( Directory( "rdd_transfer_*.*" ), {| a | FErase( a[ 1 ] ) } )
RETURN

// An error block that closes the file being read while a value too wide for the target is copied; a structure too
// big for a text file; a delimiter with a NUL in it.
STATIC PROCEDURE Hostile()
   LOCAL e
   CLOSE ALL
   dbCreate( "rdd_transfer_w", { { "AMT", "N", 5, 0 } } )
   USE rdd_transfer_w EXCLUSIVE NEW
   APPEND BLANK
   REPLACE AMT WITH 12345
   APPEND BLANK
   REPLACE AMT WITH 7
   CLOSE
   dbCreate( "rdd_transfer_n", { { "AMT", "N", 3, 0 } } )
   USE rdd_transfer_n EXCLUSIVE NEW
   ErrorBlock( {| e | QOut( "ERR", e:subCode, e:description, Alias() ), dbCloseArea(), .F. } )
   APPEND FROM rdd_transfer_w
   ? "source closed by the error block:", Alias(), LastRec(), AMT, Used()
   CLOSE ALL
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      __dbTransOpen( "rdd_transfer_v", "SDF", .T., { { "A", "C", 4294967295, 0 }, { "B", "C", 2, 0 } } )
   RECOVER USING e
      ? "too big for a text file:", e:subSystem, e:subCode, File( "rdd_transfer_v.txt" )
   END SEQUENCE
   USE rdd_transfer_w NEW
   COPY TO rdd_transfer_u DELIMITED WITH ( "TAB" + Chr( 0 ) )
   ? "a NUL in the delimiter:", Shown( "rdd_transfer_u.txt" )
   CLOSE ALL
RETURN

STATIC PROCEDURE Tables1()
   Make1()
   TablesRun()
RETURN

// NAME C10, AMT N8.2, D, L: Alpha 12.5; Beta with stars in AMT; Gamma deleted, D and L blank; a blank record; Ep"s,il
STATIC PROCEDURE Make1()
   dbCreate( "rdd_transfer_s", { { "NAME", "C", 10, 0 }, { "AMT", "N", 8, 2 }, { "D", "D", 8, 0 }, ;
      { "L", "L", 1, 0 } } )
   USE rdd_transfer_s EXCLUSIVE NEW ALIAS src
   APPEND BLANK
   REPLACE NAME WITH "Alpha", AMT WITH 12.5, D WITH SToD( "20240131" ), L WITH .T.
   APPEND BLANK
   ErrorBlock( {|| .F. } ) // the stars stay
   REPLACE NAME WITH "Beta", AMT WITH 12345678, D WITH SToD( "19991231" ), L WITH .F.
   APPEND BLANK
   REPLACE NAME WITH "Gamma", AMT WITH 1000
   DELETE
   APPEND BLANK
   APPEND BLANK
   REPLACE NAME WITH 'Ep"s,il xy', AMT WITH 99999.99, D WITH SToD( "20000229" ), L WITH .T.
   CLOSE
RETURN

STATIC PROCEDURE TablesRun()
   LOCAL e
   USE rdd_transfer_s NEW ALIAS src
   COPY TO rdd_transfer_r1
   ? "copy all:", Records( "rdd_transfer_r1.dbf" ), Eof(), RecNo()
   COPY TO rdd_transfer_r2 FIELDS amt, Name, nonexist, src->d, field->l, name
   ? "fields:", Records( "rdd_transfer_r2.dbf" )
   SET DELETED ON
   GO 3
   COPY TO rdd_transfer_r3 NEXT 2
   ? "next from deleted:", Records( "rdd_transfer_r3.dbf" ), RecNo()
   COPY TO rdd_transfer_r4 FOR AMT > 100
   ? "for:", Records( "rdd_transfer_r4.dbf" )
   SET DELETED OFF
   COPY TO rdd_transfer_r5 WHILE AMT < 99999 RECORD 2
   ? "record:", Records( "rdd_transfer_r5.dbf" ), RecNo()
   CLOSE
   dbCreate( "rdd_transfer_t", { { "NAME", "C", 5, 0 }, { "AMT", "N", 10, 4 }, { "D", "C", 8, 0 }, ;
      { "NOTE", "M", 10, 0 }, { "EXTRA", "C", 3, 0 } } )
   USE rdd_transfer_t EXCLUSIVE NEW ALIAS tgt
   ErrorBlock( {| e | QOut( "ERR", e:subSystem, e:genCode, e:subCode, e:description, e:operation ), .F. } )
   APPEND FROM rdd_transfer_s FOR AMT > 0
   ? "append:", LastRec(), RecNo(), Alias(), Select()
   GO TOP
   DO WHILE ! Eof()
      ? RecNo(), "[" + NAME + "]", AMT, "[" + D + "]", "[" + NOTE + "]", Deleted()
      SKIP
   ENDDO
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      APPEND FROM rdd_transfer_none
   RECOVER USING e
      ? "missing:", e:subSystem, e:genCode, e:subCode, e:filename
   END SEQUENCE
   CLOSE
   BEGIN SEQUENCE
      COPY TO rdd_transfer_x
   RECOVER USING e
      ? "no table:", e:subSystem, e:genCode, e:subCode, e:operation
   END SEQUENCE
RETURN

// The records of a table, read back: number, deleted mark and bytes.
STATIC FUNCTION Records( cFile )
   LOCAL c := FileText( cFile ), nHdr := Bin2W( SubStr( c, 9, 2 ) ), nLen := Bin2W( SubStr( c, 11, 2 ) ), ;
      nCount := Bin2L( SubStr( c, 5, 4 ) ), i, cOut := LTrim( Str( nCount ) ) + " " + Fields( c, nHdr )
   FOR i := 1 TO nCount
      cOut += " [" + SubStr( c, nHdr + 1 + ( i - 1 ) * nLen, nLen ) + "]"
   NEXT
RETURN cOut

STATIC FUNCTION Fields( c, nHdr )
   LOCAL cOut := "", i
   FOR i := 33 TO nHdr - 32 STEP 32
      cOut += RTrim( StrTran( SubStr( c, i, 11 ), Chr( 0 ), " " ) ) + SubStr( c, i + 11, 1 ) + ;
         LTrim( Str( Asc( SubStr( c, i + 16, 1 ) ) ) ) + "." + LTrim( Str( Asc( SubStr( c, i + 17, 1 ) ) ) ) + " "
   NEXT
RETURN cOut

STATIC FUNCTION FileText( cFile )
   LOCAL h := FOpen( cFile ), n := FSeek( h, 0, 2 ), c := Space( n )
   FSeek( h, 0 )
   FRead( h, @c, n )
   FClose( h )
RETURN c

STATIC PROCEDURE Texts()
   Make2()
   Export()
   Import()
RETURN

STATIC PROCEDURE Make2()
   dbCreate( "rdd_transfer_s", { { "NAME", "C", 10, 0 }, { "AMT", "N", 8, 2 }, { "D", "D", 8, 0 }, ;
      { "L", "L", 1, 0 } } )
   USE rdd_transfer_s EXCLUSIVE NEW ALIAS src
   APPEND BLANK
   REPLACE NAME WITH "Alpha", AMT WITH 12.5, D WITH SToD( "20240131" ), L WITH .T.
   APPEND BLANK
   ErrorBlock( {|| .F. } ) // the stars stay
   REPLACE NAME WITH "Beta", AMT WITH 12345678, D WITH SToD( "19991231" ), L WITH .F.
   APPEND BLANK
   REPLACE NAME WITH "Gamma", AMT WITH 1000
   DELETE
   APPEND BLANK
   REPLACE NAME WITH "  lead sp"
   APPEND BLANK
   REPLACE NAME WITH 'Ep"s,il xy', AMT WITH 99999.99, D WITH SToD( "20000229" ), L WITH .T.
   CLOSE
RETURN

STATIC PROCEDURE Export()
   USE rdd_transfer_s NEW ALIAS src
   COPY TO rdd_transfer_o1 SDF
   ? "sdf:", Shown( "rdd_transfer_o1.txt" )
   COPY TO rdd_transfer_o2 SDF FIELDS l, d, amt, name
   ? "sdf fields:", Shown( "rdd_transfer_o2.txt" )
   COPY TO rdd_transfer_o3 DELIMITED
   ? "delimited:", Shown( "rdd_transfer_o3.txt" )
   COPY TO rdd_transfer_o4 DELIMITED WITH BLANK
   ? "blank:", Shown( "rdd_transfer_o4.txt" )
   COPY TO rdd_transfer_o5 DELIMITED WITH TAB
   ? "tab:", Shown( "rdd_transfer_o5.txt" )
   COPY TO rdd_transfer_o6 DELIMITED WITH '|'
   ? "bar:", Shown( "rdd_transfer_o6.txt" )
   COPY TO rdd_transfer_o7 DELIMITED WITH ( { "", ";" } )
   ? "array:", Shown( "rdd_transfer_o7.txt" )
   SET DELETED ON
   COPY TO rdd_transfer_o8.csv DELIMITED FOR AMT > 0
   ? "deleted, for:", Shown( "rdd_transfer_o8.csv" )
   SET DELETED OFF
   COPY TO rdd_transfer_o9 SDF FOR .F.
   ? "none:", Shown( "rdd_transfer_o9.txt" )
   CLOSE
RETURN

STATIC PROCEDURE Import()
   LOCAL c
   dbCreate( "rdd_transfer_t", { { "NAME", "C", 10, 0 }, { "AMT", "N", 8, 2 }, { "D", "D", 8, 0 }, ;
      { "L", "L", 1, 0 } } )
   USE rdd_transfer_t EXCLUSIVE NEW ALIAS tgt
   c := "Line one     12.5020240131T" + Chr( 13 ) + Chr( 10 ) + "Short" + Chr( 10 ) + Chr( 10 ) + ;
      "Lf cr      1000.00        F" + Chr( 10 ) + Chr( 13 ) + "Too long   99999.9920000229Txxxxxx" + Chr( 13 ) + ;
      "Mid" + Chr( 26 ) + "dle        3.00        Y" + Chr( 10 ) + "Last" + Chr( 26 )
   Put( "rdd_transfer_i1.txt", c )
   APPEND FROM rdd_transfer_i1 SDF
   ? "sdf in:", LastRec(), RecNo()
   Dump()
   ZAP
   c := '"Alpha",12.5,20240131,T' + Chr( 10 ) + '  "Lead",  -3.25 ,  19991231  , y' + Chr( 10 ) + ;
      'Unq uoted,7,20240229,Y' + Chr( 10 ) + '"Has,comma",1,x,N' + Chr( 10 ) + '"Emb"ed"x,2,,t' + Chr( 10 ) + ;
      '"VeryLongNameOverflow",12345,20240131,TRUE' + Chr( 10 ) + '"Short"' + Chr( 10 ) + Chr( 10 ) + ;
      '"A","5","20240101","T",extra' + Chr( 10 ) + ',,,' + Chr( 10 ) + '"Open quote,9,20240101,T' + Chr( 10 ) + ;
      'y,12abc,20240101,?' + Chr( 10 ) + 'z,99999999,20240101,T' + Chr( 13 ) + 'last,-0.005,20240101,F'
   Put( "rdd_transfer_i2.txt", c )
   APPEND FROM rdd_transfer_i2 DELIMITED
   ? "delimited in:", LastRec()
   Dump()
   ZAP
   Put( "rdd_transfer_i3.txt", "Two  words 1 20240101 T" + Chr( 10 ) + "Ab 4 20240101 y" + Chr( 10 ) )
   APPEND FROM rdd_transfer_i3 DELIMITED WITH BLANK
   Dump()
   ZAP
   APPEND FROM rdd_transfer_o1 SDF FOR AMT < 100 .AND. ! Empty( NAME )
   ? "for in the text:"
   Dump()
   ZAP
   APPEND FROM rdd_transfer_o1 SDF NEXT 2
   ? "next:", LastRec()
   ZAP
   APPEND FROM rdd_transfer_i1 SDF
   APPEND FROM rdd_transfer_i2 DELIMITED
   CLOSE
   ? "bytes:", TableBytes( "rdd_transfer_t.dbf" )
RETURN

STATIC FUNCTION TableBytes( cFile )
   LOCAL h := FOpen( cFile ), n := FSeek( h, 0, 2 ), c := Space( n ), nHdr, nLen, nCount, i, cOut
   FSeek( h, 0 )
   FRead( h, @c, n )
   FClose( h )
   nHdr := Bin2W( SubStr( c, 9, 2 ) )
   nLen := Bin2W( SubStr( c, 11, 2 ) )
   nCount := Bin2L( SubStr( c, 5, 4 ) )
   cOut := LTrim( Str( nCount ) )
   FOR i := 1 TO nCount
      cOut += Chr( 10 ) + "[" + SubStr( c, nHdr + 1 + ( i - 1 ) * nLen, nLen ) + "]"
   NEXT
RETURN cOut

STATIC PROCEDURE Dump()
   LOCAL n := RecNo()
   GO TOP
   DO WHILE ! Eof()
      ? " ", RecNo(), "[" + Bytes() + "]"
      SKIP
   ENDDO
   GO n
RETURN

// The current record's bytes, as the table keeps them.
STATIC FUNCTION Bytes()
   LOCAL c := "", i
   FOR i := 1 TO FCount()
      c += Transform( FieldGet( i ), "" ) + "|"
   NEXT
RETURN c

STATIC PROCEDURE Put( cFile, c )
   LOCAL h := FCreate( cFile )
   FWrite( h, c )
   FClose( h )
RETURN

// A file's bytes, the line ends and other control characters shown.
STATIC FUNCTION Shown( cFile )
   LOCAL h := FOpen( cFile ), n := FSeek( h, 0, 2 ), c := Space( n ), i, cOut := "", ch
   FSeek( h, 0 )
   FRead( h, @c, n )
   FClose( h )
   c := StrTran( c, EOL, "<EOL>" )
   FOR i := 1 TO Len( c )
      ch := SubStr( c, i, 1 )
      cOut += iif( Asc( ch ) < 32, "<" + LTrim( Str( Asc( ch ) ) ) + ">", ch )
   NEXT
RETURN cOut

STATIC PROCEDURE Structures()
   LOCAL e, i
   dbCreate( "rdd_transfer_m", { { "NAME", "C", 10, 0 }, { "AMT", "N", 6, 1 }, { "NOTE", "M", 10, 0 }, ;
      { "BIG", "C", 300, 0 } } )
   USE rdd_transfer_m EXCLUSIVE NEW
   FOR i := 1 TO 4
      APPEND BLANK
      REPLACE NAME WITH "n" + Str( i, 1 ), AMT WITH i * 1.5, NOTE WITH Replicate( "m", i )
   NEXT
   GO 2
   COPY TO rdd_transfer_a SDF
   ? "memo sdf:", Shown( "rdd_transfer_a.txt" )
   GO 2
   COPY TO rdd_transfer_b DELIMITED WITH BLANK NEXT 2
   ? "with, then next:", Shown( "rdd_transfer_b.txt" ), RecNo()
   COPY TO rdd_transfer_c VIA "DBFCDX" FOR AMT > 2
   USE rdd_transfer_c VIA "DBFCDX" NEW
   ? "cdx copy:", RddName(), LastRec(), NAME, NOTE, File( "rdd_transfer_c.fpt" )
   CLOSE
   SELECT rdd_transfer_m
   COPY STRUCTURE TO rdd_transfer_d FIELDS note, zz, amt, Name
   USE rdd_transfer_d NEW
   ? "structure:", LastRec(), FCount(), FieldName( 1 ), FieldName( 2 ), FieldName( 3 )
   CLOSE
   SELECT rdd_transfer_m
   COPY STRUCTURE EXTENDED TO rdd_transfer_e
   USE rdd_transfer_e NEW
   DO WHILE ! Eof()
      ? " ", FIELD_NAME, FIELD_TYPE, FIELD_LEN, FIELD_DEC
      SKIP
   ENDDO
   CLOSE
   CREATE rdd_transfer_f FROM rdd_transfer_e NEW
   ? "create from:", Alias(), FCount(), dbStruct()[ 4, 3 ], dbStruct()[ 4, 4 ], Select()
   CLOSE
   CREATE rdd_transfer_g
   ? "create:", Alias(), FCount(), FieldName( 1 ), LastRec()
   CLOSE ALL
   ? "table extension:", dbTableExt()
   USE rdd_transfer_m SHARED NEW
   APPEND FROM rdd_transfer_m
   ? "itself:", LastRec()
   ErrorBlock( {| e | Break( e ) } )
   BEGIN SEQUENCE
      COPY TO rdd_transfer_h VIA "NOSUCH"
   RECOVER USING e
      ? "unknown driver:", e:subSystem, e:genCode, e:subCode, File( "rdd_transfer_h.dbf" )
   END SEQUENCE
   CLOSE ALL
RETURN

