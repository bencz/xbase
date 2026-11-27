// Destructors run where the code tests the pending requests, after an operation that can fail (a hash literal): one
// that changes an instance variable read before such an operation is seen by the read after it (no read merged across
// it). The destructors of short-lived objects run between the two reads in some calls (which ones depends on the
// collector's pace): none of those reads is stale.
STATIC s_o, s_nRan := 0

CLASS Box
   VAR x INIT 0
ENDCLASS

CLASS Killer
   VAR pad
   DESTRUCTOR Kill
ENDCLASS

METHOD PROCEDURE Kill CLASS Killer
   s_o:x++
   s_nRan++
RETURN

PROCEDURE Main()
   LOCAL i, nStale := 0, nSeen := 0, k
   FOR i := 1 TO 200000
      k := Killer():New()
      k:pad := Space( 200 )
      k := NIL
      Probe( i, @nStale, @nSeen )
   NEXT
   ? "ran between:", IIf( nSeen > 0, "yes", "no" ), "stale:", nStale
RETURN

STATIC PROCEDURE Probe( i, nStale, nSeen )
   LOCAL o := Box():New(), n, m, h, nBefore, nAfter
   s_o := o
   n := o:x
   nBefore := s_nRan
   h := { "a" => i, "b" => Space( 100 ) }
   nAfter := s_nRan
   m := o:x
   IF nAfter != nBefore
      nSeen++
      IF m == n
         nStale++
      ENDIF
   ENDIF
RETURN
