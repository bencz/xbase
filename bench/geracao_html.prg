// HTML generation: a table of 100,000 items { name of two words, quantity, price in cents } made from a fixed seed,
// then timed: one page built by appending to a string, each row with its id, a zebra class, the name escaped (StrTran
// of & < > "), and the price and total formatted with Str. The page's length, a row and its end are checked.
STATIC nSeed := 4711

PROCEDURE Main()
   LOCAL aWords := { "Alpha", "Beta & Sons", "<Gamma>", "Delta", 'Echo "Q"', "Foxtrot", "Golf", "H&M" }
   LOCAL aItems := Array( 100000 ), t, i, r, c, cRow, cId, nPos77, cRow77, cTail, ok
   FOR i := 1 TO Len( aItems )
      aItems[ i ] := { aWords[ Rand( 8 ) + 1 ] + " " + aWords[ Rand( 8 ) + 1 ], Rand( 100 ) + 1, Rand( 1000000 ) + 1 }
   NEXT
   t := ElapsedMs()
   c := "<html><head><title>Items</title></head><body>" + Chr( 10 ) + "<table>" + Chr( 10 ) + ;
        "<tr><th>Id</th><th>Name</th><th>Qty</th><th>Price</th><th>Total</th></tr>" + Chr( 10 )
   FOR i := 1 TO Len( aItems )
      r := aItems[ i ]
      cId := LTrim( Str( i ) )
      IF i == 77777
         nPos77 := Len( c ) + 1
      ENDIF
      cRow := '<tr id="r' + cId + '" class="' + IIf( i % 2 == 1, "odd", "even" ) + '"><td>' + cId + "</td><td>" + ;
           Escape( r[ 1 ] ) + '</td><td class="n">' + LTrim( Str( r[ 2 ], 10, 0 ) ) + '</td><td class="n">' + ;
           LTrim( Str( r[ 3 ] / 100, 12, 2 ) ) + '</td><td class="n">' + ;
           LTrim( Str( r[ 2 ] * r[ 3 ] / 100, 14, 2 ) ) + "</td></tr>" + Chr( 10 )
      c += cRow
   NEXT
   c += "</table>" + Chr( 10 ) + "</body></html>" + Chr( 10 )
   cRow77 := '<tr id="r77777" class="odd"><td>77777</td><td>&lt;Gamma&gt; Delta</td><td class="n">56</td>' + ;
             '<td class="n">3992.87</td><td class="n">223600.72</td></tr>' + Chr( 10 )
   cTail := '<tr id="r100000" class="even"><td>100000</td><td>Foxtrot Alpha</td><td class="n">57</td>' + ;
            '<td class="n">4506.67</td><td class="n">256880.19</td></tr>' + Chr( 10 ) + "</table>" + Chr( 10 ) + ;
            "</body></html>" + Chr( 10 )
   ok := Len( c ) == 15121698 .AND. nPos77 == 11756338 .AND. SubStr( c, nPos77, Len( cRow77 ) ) == cRow77 .AND. ;
         Right( c, Len( cTail ) ) == cTail
   ? "geracao_html", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
RETURN

STATIC FUNCTION Escape( c )
RETURN StrTran( StrTran( StrTran( StrTran( c, "&", "&amp;" ), "<", "&lt;" ), ">", "&gt;" ), '"', "&quot;" )

STATIC FUNCTION Rand( n )
   nSeed := nSeed * 16807 % 2147483647
RETURN nSeed % n
