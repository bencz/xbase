// A singly linked list of two-element arrays { value, next }: 1,000,000 nodes pushed at the head, walked, reversed in
// place, walked again, and every second node unlinked; 3 rounds.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), r, i, head, node, prev, next, nSum := 0, nCount := 0, nFirst := 0, ok
   FOR r := 1 TO 3
      head := NIL
      FOR i := 1 TO 1000000
         head := { i, head }
      NEXT
      node := head
      DO WHILE node != NIL
         nSum += node[ 1 ]
         node := node[ 2 ]
      ENDDO
      prev := NIL
      node := head
      DO WHILE node != NIL
         next := node[ 2 ]
         node[ 2 ] := prev
         prev := node
         node := next
      ENDDO
      head := prev
      nFirst += head[ 1 ]
      node := head
      DO WHILE node != NIL .AND. node[ 2 ] != NIL
         node[ 2 ] := node[ 2 ][ 2 ]
         node := node[ 2 ]
      ENDDO
      node := head
      DO WHILE node != NIL
         nCount++
         node := node[ 2 ]
      ENDDO
   NEXT
   ok := nSum == 1500001500000 .AND. nFirst == 3 .AND. nCount == 1500000
   ? "linked_list", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN
