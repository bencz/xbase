// The n-body simulation (the benchmarks game's): the Sun and the four giant planets, 500,000 steps of 0.01 days with
// doubles in arrays (one per coordinate), the system's energy before and after.
PROCEDURE Main()
   LOCAL t := ElapsedMs(), steps := 500000, nb := 5, i, j, k, e0, e1
   LOCAL pi := Pi(), solarMass, days := 365.24, dt := 0.01
   LOCAL dx, dy, dz, d2, mag, mi, mj, px := 0.0, py := 0.0, pz := 0.0
   LOCAL x, y, z, vx, vy, vz, m, ok
   solarMass := 4.0 * pi * pi
   x := { 0.0, 4.84143144246472090, 8.34336671824457987, 12.8943695621391310, 15.3796971148509165 }
   y := { 0.0, -1.16032004402742839, 4.12479856412430479, -15.1111514016986312, -25.9193146099879641 }
   z := { 0.0, -0.103622044471123109, -0.403523417114321381, -0.223307578892655734, 0.179258772950371181 }
   vx := { 0.0, 0.00166007664274403694 * days, -0.00276742510726862411 * days, 0.00296460137564761618 * days, ;
      0.00268067772490389322 * days }
   vy := { 0.0, 0.00769901118419740425 * days, 0.00499852801234917238 * days, 0.00237847173959480950 * days, ;
      0.00162824170038242295 * days }
   vz := { 0.0, -0.0000690460016972063023 * days, 0.0000230417297573763929 * days, ;
      -0.0000296589568540237556 * days, -0.0000951592254519715870 * days }
   m := { solarMass, 0.000954791938424326609 * solarMass, 0.000285885980666130812 * solarMass, ;
      0.0000436624404335156298 * solarMass, 0.0000515138902046611451 * solarMass }
   FOR i := 1 TO nb
      px += vx[ i ] * m[ i ]
      py += vy[ i ] * m[ i ]
      pz += vz[ i ] * m[ i ]
   NEXT
   vx[ 1 ] := -px / solarMass
   vy[ 1 ] := -py / solarMass
   vz[ 1 ] := -pz / solarMass
   e0 := Energy( x, y, z, vx, vy, vz, m )
   FOR k := 1 TO steps
      FOR i := 1 TO nb
         mi := m[ i ]
         FOR j := i + 1 TO nb
            dx := x[ i ] - x[ j ]
            dy := y[ i ] - y[ j ]
            dz := z[ i ] - z[ j ]
            d2 := dx * dx + dy * dy + dz * dz
            mag := dt / ( d2 * Sqrt( d2 ) )
            mj := m[ j ]
            vx[ i ] -= dx * mj * mag
            vy[ i ] -= dy * mj * mag
            vz[ i ] -= dz * mj * mag
            vx[ j ] += dx * mi * mag
            vy[ j ] += dy * mi * mag
            vz[ j ] += dz * mi * mag
         NEXT
      NEXT
      FOR i := 1 TO nb
         x[ i ] += dt * vx[ i ]
         y[ i ] += dt * vy[ i ]
         z[ i ] += dt * vz[ i ]
      NEXT
   NEXT
   e1 := Energy( x, y, z, vx, vy, vz, m )
   ok := Abs( e0 + 0.169075164 ) < 0.000000001 .AND. Abs( e1 + 0.169096567 ) < 0.000000001
   ? "n_body", ElapsedMs() - t, IIf( ok, "ok", "WRONG" )
   RETURN

STATIC FUNCTION Energy( x, y, z, vx, vy, vz, m )
   LOCAL e := 0.0, i, j, dx, dy, dz
   FOR i := 1 TO Len( m )
      e += 0.5 * m[ i ] * ( vx[ i ] * vx[ i ] + vy[ i ] * vy[ i ] + vz[ i ] * vz[ i ] )
      FOR j := i + 1 TO Len( m )
         dx := x[ i ] - x[ j ]
         dy := y[ i ] - y[ j ]
         dz := z[ i ] - z[ j ]
         e -= m[ i ] * m[ j ] / Sqrt( dx * dx + dy * dy + dz * dz )
      NEXT
   NEXT
   RETURN e
