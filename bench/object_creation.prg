// Creating objects: 600,000 objects of a class with five variables (INIT values: numbers, a string and an array,
// which each object gets a copy of) made with New() and an Init() method that sets two of them, half of them of a
// subclass with two more variables; one in 100 kept in an array, the rest left to the collector.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), aKeep := {}, i, o, n := 0
   FOR i := 1 TO 600000
      IF i % 2 == 0
         o := TOrder():New( i, "c" + StrZero( i % 1000, 3 ) )
      ELSE
         o := TBigOrder():New( i, "c000" )
         o:nDiscount := 5
      ENDIF
      AAdd( o:aLines, i % 7 )
      IF i % 100 == 0
         AAdd( aKeep, o )
      ENDIF
   NEXT
   FOR EACH o IN aKeep
      n += o:nId + o:nTotal + Len( o:aLines ) + o:aLines[ 1 ] + Len( o:cCustomer )
   NEXT
   // the kept ones: ids 100, 200, ..., 600,000 (all even: TOrder), nTotal 0, one line of i % 7, a 4-character name;
   // 17999 is the sum of i % 7 over those ids
   ? "object_creation", ElapsedMs() - t, ;
     IIf( Len( aKeep ) == 6000 .AND. n == 1800300000 + 6000 * 5 + 17999, "ok", "WRONG" )
RETURN

CLASS TOrder
   VAR nId INIT 0
   VAR cCustomer INIT ""
   VAR nTotal INIT 0
   VAR cStatus INIT "new"
   VAR aLines INIT {}
   METHOD Init( nId, cCustomer )
ENDCLASS

METHOD Init( nId, cCustomer ) CLASS TOrder
   ::nId := nId
   ::cCustomer := cCustomer
RETURN Self

CLASS TBigOrder INHERIT TOrder
   VAR nDiscount INIT 0
   VAR dDue INIT SToD( "20261231" )
ENDCLASS
