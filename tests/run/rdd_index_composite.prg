// Orders on keys of several fields, NTX and CDX alike: NOME + CIDADE with 2, 3, 4 and 5 fields, a number through Str()
// and a date through DToC() and DToS() before a name, separators between the fields, and a key built at run time
// from a list of field names (a macro). Each order lists the records in its key's order (equal names then by city,
// state, ...); SEEK of a whole key, of the first fields padded to their width (a part of a key), of fields not padded
// (no match: a field takes its whole width in the key), with SOFTSEEK and from the last of equal keys; a REPLACE of
// one field moves its record in every open order; FOR and UNIQUE; the keys and their lengths after the table and its
// orders are opened again. The files start with "rdd_index_composite", deleted at the end.
REQUEST DBFCDX
FIELD CODIGO, NOME, CIDADE, ESTADO, BAIRRO, CEP, NUMERO, DATA

PROCEDURE Main()
   LOCAL cRdd
   SET DATE ANSI
   FOR EACH cRdd IN { "DBFNTX", "DBFCDX" }
      ? "==", cRdd
      rddSetDefault( cRdd )
      Composite( cRdd == "DBFNTX" )
   NEXT
   RETURN

STATIC PROCEDURE Composite( lNtx )
   LOCAL a := { ;
      { "000001", "JOAO DA SILVA", "OSASCO", "SP", "CENTRO", "06010000", 120, "20240315" }, ;
      { "000002", "MARIA SOUZA", "SAO PAULO", "SP", "PINHEIROS", "05422000", 7, "20231201" }, ;
      { "000003", "JOAO DA SILVA", "CAMPINAS", "SP", "CAMBUI", "13024000", 120, "20240101" }, ;
      { "000004", "ANA", "RECIFE", "PE", "BOA VIAGEM", "51020000", 3, "20220710" }, ;
      { "000005", "ANA MARIA", "RECIFE", "PE", "CASA FORTE", "52060000", 45, "20220710" }, ;
      { "000006", "JOAO DA SILVA", "OSASCO", "SP", "BELA VISTA", "06020000", 9, "20240316" }, ;
      { "000007", "MARIA SOUZA", "SAO PAULO", "RJ", "CENTRO", "20010000", 1200, "20231130" }, ;
      { "000008", "BRUNO", "CURITIBA", "PR", "BATEL", "80420000", 0, "20210505" }, ;
      { "000009", "JOAO DA SILVA", "OSASCO", "SP", "CENTRO", "06010001", -5, "20240315" }, ;
      { "000010", "ana", "recife", "pe", "boa viagem", "51020001", 3, "20220711" } }, r, i, aOrders, cExpr, aFields
   dbCreate( "rdd_index_composite", { { "CODIGO", "C", 6, 0 }, { "NOME", "C", 40, 0 }, { "CIDADE", "C", 30, 0 }, ;
      { "ESTADO", "C", 2, 0 }, { "BAIRRO", "C", 30, 0 }, { "CEP", "C", 8, 0 }, { "NUMERO", "N", 10, 0 }, ;
      { "DATA", "D", 8, 0 } } )
   USE rdd_index_composite EXCLUSIVE NEW
   FOR EACH r IN a
      APPEND BLANK
      REPLACE CODIGO WITH r[ 1 ], NOME WITH r[ 2 ], CIDADE WITH r[ 3 ], ESTADO WITH r[ 4 ], BAIRRO WITH r[ 5 ], ;
         CEP WITH r[ 6 ], NUMERO WITH r[ 7 ], DATA WITH SToD( r[ 8 ] )
   NEXT
   INDEX ON NOME + CIDADE TO rdd_index_composite_1
   INDEX ON NOME + CIDADE + ESTADO TO rdd_index_composite_2
   INDEX ON NOME + CIDADE + ESTADO + BAIRRO TO rdd_index_composite_3
   INDEX ON NOME + CIDADE + ESTADO + BAIRRO + CEP TO rdd_index_composite_4
   INDEX ON Str( NUMERO, 10, 0 ) + NOME TO rdd_index_composite_5
   INDEX ON DToC( DATA ) + NOME TO rdd_index_composite_6
   INDEX ON DToS( DATA ) + NOME TO rdd_index_composite_7
   INDEX ON NOME + "|" + CIDADE + "|" + ESTADO TO rdd_index_composite_8
   aFields := { "NOME", "CIDADE", "ESTADO", "BAIRRO", "CEP" }
   cExpr := ""
   FOR i := 1 TO Len( aFields )
      cExpr += IIf( i > 1, "+", "" ) + AllTrim( aFields[ i ] )
   NEXT
   INDEX ON &cExpr TO rdd_index_composite_9
   INDEX ON NOME + ESTADO TO rdd_index_composite_u UNIQUE
   INDEX ON NOME + CIDADE TO rdd_index_composite_f FOR ESTADO == "SP"
   aOrders := { "1", "2", "3", "4", "5", "6", "7", "8", "9", "u", "f" }
   CLOSE
   USE rdd_index_composite SHARED NEW
   SET INDEX TO rdd_index_composite_1, rdd_index_composite_2, rdd_index_composite_3, rdd_index_composite_4, ;
      rdd_index_composite_5, rdd_index_composite_6, rdd_index_composite_7, rdd_index_composite_8, ;
      rdd_index_composite_9, rdd_index_composite_u, rdd_index_composite_f
   FOR i := 1 TO Len( aOrders )
      SET ORDER TO i
      ? aOrders[ i ], IndexKey( 0 ), Len( &( IndexKey( 0 ) ) ), List()
   NEXT

   SET ORDER TO 2
   SEEK PadR( "JOAO DA SILVA", 40 ) + PadR( "OSASCO", 30 ) + "SP"
   ? "whole key:", Found(), CODIGO
   SEEK "JOAO DA SILVA" + "OSASCO" + "SP"
   ? "not padded:", Found(), Eof()
   SEEK PadR( "JOAO DA SILVA", 40 )
   ? "name only:", Found(), CODIGO
   SEEK PadR( "JOAO DA SILVA", 40 ) + PadR( "OSASCO", 30 )
   ? "name and city:", Found(), CODIGO
   ? "last of equal:", dbSeek( PadR( "JOAO DA SILVA", 40 ) + PadR( "OSASCO", 30 ), .F., .T. ), CODIGO
   SET SOFTSEEK ON
   SEEK PadR( "JOAO", 40 )
   ? "soft:", Found(), CODIGO
   SET SOFTSEEK OFF
   SEEK "JOAO"
   ? "prefix:", Found(), CODIGO
   SET ORDER TO 5
   SEEK Str( 120, 10, 0 )
   ? "number:", Found(), CODIGO
   SET ORDER TO 7
   SEEK "20231130"
   ? "date:", Found(), CODIGO
   SET ORDER TO 8
   SEEK PadR( "ANA MARIA", 40 ) + "|"
   ? "separator:", Found(), CODIGO

   GO TOP
   LOCATE FOR CODIGO == "000003"
   RLock()
   REPLACE CIDADE WITH "ZZZ", NOME WITH "ANA"
   dbUnlock()
   ? "after replace:"
   FOR i := 1 TO Len( aOrders )
      SET ORDER TO i
      ?? "", aOrders[ i ] + ":" + List()
   NEXT
   CLOSE

   USE rdd_index_composite NEW
   SET INDEX TO rdd_index_composite_4, rdd_index_composite_9
   ? "reopened:", IndexKey( 1 ), ordKey( 2 ), IndexKey( 1 ) == ordKey( 2 ), List()
   SEEK PadR( "MARIA SOUZA", 40 ) + PadR( "SAO PAULO", 30 ) + "RJ"
   ? "found again:", Found(), CODIGO
   CLOSE
   FErase( "rdd_index_composite.dbf" )
   FOR EACH r IN aOrders
      FErase( "rdd_index_composite_" + r + IIf( lNtx, ".ntx", ".cdx" ) )
   NEXT
   RETURN

// The records' codes in the controlling order, from the top.
STATIC FUNCTION List()
   LOCAL c := ""
   GO TOP
   DO WHILE ! Eof()
      c += Right( CODIGO, 2 ) + " "
      SKIP
   ENDDO
   RETURN RTrim( c )
