// bench/lru_cache.prg in Harbour (its hash functions where xbase has its own names).
// A least-recently-used cache of 2,000 entries: a hash from key to node and a doubly linked list of nodes
// { key, value, prev, next } in order of use; 2,000,000 reads of keys skewed toward small numbers, a miss computing
// the value and evicting the oldest entry when the cache is full.
#define N_KEY   1
#define N_VALUE 2
#define N_PREV  3
#define N_NEXT  4

STATIC s_nSeed := 1234

PROCEDURE Main()
   LOCAL t := ElapsedMs(), h := { => }, aHead := { NIL, NIL, NIL, NIL }, aTail := { NIL, NIL, NIL, NIL }, ok
   LOCAL i, k, node, nHits := 0, nSum := 0, nCap := 2000
   aHead[ N_NEXT ] := aTail
   aTail[ N_PREV ] := aHead
   FOR i := 1 TO 2000000
      k := Rand( Rand( 20000 ) + 1 )
      IF k $ h
         nHits++
         node := h[ k ]
         node[ N_PREV ][ N_NEXT ] := node[ N_NEXT ]
         node[ N_NEXT ][ N_PREV ] := node[ N_PREV ]
      ELSE
         IF Len( h ) >= nCap
            node := aTail[ N_PREV ]
            node[ N_PREV ][ N_NEXT ] := aTail
            aTail[ N_PREV ] := node[ N_PREV ]
            hb_HDel( h, node[ N_KEY ] )
         ENDIF
         node := { k, k * 3 + 1, NIL, NIL }
         h[ k ] := node
      ENDIF
      node[ N_NEXT ] := aHead[ N_NEXT ]
      node[ N_PREV ] := aHead
      aHead[ N_NEXT ][ N_PREV ] := node
      aHead[ N_NEXT ] := node
      nSum += node[ N_VALUE ]
   NEXT
   ok := nHits == 378837 .AND. nSum == 30033837356 .AND. Len( h ) == nCap
   ? "lru_cache", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Rand( n )
   s_nSeed := ( s_nSeed * 16807 ) % 2147483647
   RETURN s_nSeed % n
