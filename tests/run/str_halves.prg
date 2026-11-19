// Str() and Transform() round a double's decimals as Clipper does: the fraction's digits come one at a time from the
// fraction times ten, and a 4 followed by nines up to the 16 significant digits counts as a 5. So a decimal half
// stored just below it (8904.185 is 8904.18499999999949...) rounds up, as the reference shows it, while another one
// a little further below (9832.585, 64.245) rounds down; Round() has its own rule. The digits are the exact ones,
// where Clipper's products by ten drift from the 13th decimal on (0.9469037222444915 is not ...914 nor ...916).
PROCEDURE Main()
   LOCAL a := { 8904.185, -8882.925, 9832.585, 64.245, 1.005, 2.675, 0.125, 1.115, 1234567.885, 8.995, 1.4650, ;
                2.195, 9.495, 16.065, 0.045, 100.005, 33.335, 7.775, 0.0005, 123456789012.345 }, v
   FOR EACH v IN a
      ? Str( v, 18, 2 ), Str( v, 18, 3 ), Str( v, 18, 1 ), Str( v, 18, 0 ), Transform( v, "@E 999,999,999,999.99" ), ;
        Round( v, 2 )
   NEXT
   FOR EACH v IN { 8528950501312055 / 2 ^ 53, 1 / 3, 2 / 3, 1 / 7, 0.1, 5 / 9 }
      ? Str( v, 18, 16 ), Str( v, 18, 15 ), Str( v, 18, 14 ), Str( v, 18, 13 )
   NEXT
   RETURN
