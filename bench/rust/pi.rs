// bench/pi.prg in Rust, translated statement by statement: the tape machine's digits of pi.
use std::time::Instant;

fn pi_digits(digits: i64) -> String {
    let mut x = vec![0i64; 128001];
    let mut xc: usize = 1;
    let mut out = String::new();
    xc += 1;
    x[xc] += digits;
    while x[xc] != 0 {
      xc -= 1;
      x[xc] += 1;
      xc += 8;
      x[xc] += 10;
      xc -= 7;
      x[xc] -= 1;
    }
    xc += 1;
    x[xc] += 5;
    while x[xc] != 0 {
      xc -= 1;
      x[xc] += 9;
      xc += 1;
      x[xc] -= 1;
    }
    x[xc] += 1;
    xc += 6;
    x[xc] += 1;
    while x[xc] != 0 {
      xc -= 2;
      x[xc] += 3;
      while x[xc] != 0 {
        xc += 2;
        while x[xc] != 0 {
          x[xc] -= 1;
          xc -= 1;
        }
        xc -= 1;
        while x[xc] != 0 {
          xc += 1;
        }
        xc -= 1;
        x[xc] -= 1;
      }
      xc += 2;
      while x[xc] != 0 {
        xc += 1;
        x[xc] += 1;
        xc += 1;
      }
      xc -= 1;
      while x[xc] != 0 {
        xc -= 1;
      }
      xc += 1;
    }
    xc += 1;
    while x[xc] != 0 {
      while x[xc] != 0 {
        x[xc] -= 1;
        xc += 4;
        x[xc] += 1;
        xc -= 4;
      }
      xc += 3;
      x[xc] += 3;
      xc += 1;
      x[xc] -= 1;
    }
    xc -= 1;
    while x[xc] != 0 {
      xc -= 4;
    }
    xc -= 8;
    x[xc] += 1;
    while x[xc] != 0 {
      x[xc] -= 1;
      xc += 12;
      while x[xc] != 0 {
        xc -= 1;
        x[xc] += 1;
        while x[xc] != 0 {
          x[xc] -= 1;
          xc += 4;
          x[xc] += 1;
          xc -= 4;
        }
        xc += 1;
        xc += 4;
      }
      xc -= 4;
      while x[xc] != 0 {
        xc += 5;
        while x[xc] != 0 {
          xc -= 4;
          x[xc] += 1;
          xc += 4;
          x[xc] -= 1;
        }
        xc -= 5;
        x[xc] -= 1;
        while x[xc] != 0 {
          xc -= 2;
          x[xc] += 10;
          xc += 2;
          x[xc] -= 1;
        }
        xc += 3;
        while x[xc] != 0 {
          xc -= 2;
          while x[xc] != 0 {
            xc -= 1;
            x[xc] += 1;
            xc -= 2;
            x[xc] += 1;
            xc += 3;
            x[xc] -= 1;
          }
          xc -= 1;
          while x[xc] != 0 {
            xc += 1;
            x[xc] += 1;
            xc -= 1;
            x[xc] -= 1;
          }
          xc -= 1;
          x[xc] += 2;
          xc -= 2;
          x[xc] += 1;
          xc += 6;
          x[xc] -= 1;
        }
        xc -= 2;
        while x[xc] != 0 {
          x[xc] -= 1;
        }
        xc -= 2;
        x[xc] -= 1;
        xc -= 1;
        while x[xc] != 0 {
          x[xc] -= 1;
          xc += 2;
          x[xc] += 1;
          xc -= 1;
          x[xc] -= 1;
          while x[xc] != 0 {
            xc += 3;
          }
          xc += 1;
          while x[xc] != 0 {
            while x[xc] != 0 {
              xc -= 1;
              x[xc] += 1;
              xc += 1;
              x[xc] -= 1;
            }
            xc += 1;
            x[xc] += 1;
            xc += 2;
          }
          xc -= 5;
        }
        xc += 1;
        while x[xc] != 0 {
          x[xc] -= 1;
        }
        xc += 1;
        x[xc] += 1;
        xc -= 3;
        x[xc] -= 1;
        while x[xc] != 0 {
          xc += 2;
          x[xc] += 1;
          xc -= 2;
          x[xc] -= 1;
        }
        xc -= 1;
      }
      xc -= 4;
      x[xc] += 1;
      xc += 7;
      xc += 1;
      while x[xc] != 0 {
        x[xc] -= 1;
      }
      xc += 1;
      while x[xc] != 0 {
        xc -= 3;
        x[xc] += 1;
        xc += 3;
        x[xc] -= 1;
      }
      xc -= 2;
      x[xc] += 10;
      xc -= 1;
      while x[xc] != 0 {
        x[xc] -= 1;
        xc += 2;
        x[xc] += 1;
        xc -= 1;
        x[xc] -= 1;
        while x[xc] != 0 {
          xc += 3;
        }
        xc += 1;
        while x[xc] != 0 {
          while x[xc] != 0 {
            xc -= 1;
            x[xc] += 1;
            xc += 1;
            x[xc] -= 1;
          }
          xc += 1;
          x[xc] += 1;
          xc += 2;
        }
        xc -= 5;
      }
      xc += 1;
      while x[xc] != 0 {
        x[xc] -= 1;
      }
      xc += 1;
      x[xc] += 1;
      xc += 1;
      while x[xc] != 0 {
        xc -= 2;
        x[xc] += 1;
        xc -= 1;
        x[xc] += 1;
        xc += 3;
        x[xc] -= 1;
      }
      xc -= 3;
      xc -= 1;
      x[xc] += 1;
      xc -= 1;
      x[xc] += 1;
      xc += 2;
      while x[xc] != 0 {
        x[xc] -= 1;
        while x[xc] != 0 {
          x[xc] -= 1;
          while x[xc] != 0 {
            x[xc] -= 1;
            while x[xc] != 0 {
              x[xc] -= 1;
              while x[xc] != 0 {
                x[xc] -= 1;
                while x[xc] != 0 {
                  x[xc] -= 1;
                  while x[xc] != 0 {
                    x[xc] -= 1;
                    while x[xc] != 0 {
                      x[xc] -= 1;
                      while x[xc] != 0 {
                        x[xc] -= 1;
                        xc -= 1;
                        x[xc] -= 1;
                        xc += 1;
                        while x[xc] != 0 {
                          x[xc] -= 1;
                          xc -= 1;
                          x[xc] += 1;
                          xc -= 1;
                          x[xc] -= 1;
                          xc += 2;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      xc -= 1;
      while x[xc] != 0 {
        x[xc] += 5;
        while x[xc] != 0 {
          xc -= 3;
          x[xc] += 8;
          xc -= 1;
          x[xc] += 8;
          xc += 4;
          x[xc] -= 1;
        }
        xc -= 1;
        xc -= 3;
        x[xc] += 1;
        xc -= 1;
        x[xc] -= 1;
        xc += 4;
        while x[xc] != 0 {
          xc += 1;
          x[xc] += 1;
          xc -= 3;
          x[xc] += 9;
          xc -= 1;
          x[xc] -= 1;
          xc += 3;
          x[xc] -= 1;
        }
        xc -= 5;
        while x[xc] != 0 {
          xc += 2;
          x[xc] += 1;
          xc -= 2;
          x[xc] -= 1;
        }
        x[xc] += 1;
        xc -= 1;
        while x[xc] != 0 {
          x[xc] -= 1;
          xc += 1;
          x[xc] -= 1;
          xc -= 1;
        }
        xc += 1;
        while x[xc] != 0 {
          xc += 2;
          out.push(x[xc] as u8 as char);
          xc -= 4;
          while x[xc] != 0 {
            x[xc] += 1;
            out.push(x[xc] as u8 as char);
            while x[xc] != 0 {
              x[xc] -= 1;
            }
          }
          xc += 2;
          x[xc] -= 1;
        }
        xc += 1;
        while x[xc] != 0 {
          xc += 2;
          out.push(x[xc] as u8 as char);
          xc -= 2;
          x[xc] -= 1;
        }
        xc += 1;
        while x[xc] != 0 {
          x[xc] -= 1;
        }
        xc += 1;
        while x[xc] != 0 {
          x[xc] -= 1;
        }
        xc += 3;
        while x[xc] != 0 {
          xc += 2;
          while x[xc] != 0 {
            xc -= 8;
            x[xc] += 1;
            xc += 8;
            x[xc] -= 1;
          }
          xc -= 2;
          x[xc] -= 1;
        }
      }
      xc += 2;
      while x[xc] != 0 {
        x[xc] -= 1;
      }
      xc -= 3;
      while x[xc] != 0 {
        x[xc] -= 1;
      }
      xc -= 8;
    }
    x[xc] += 10;
    out.push(x[xc] as u8 as char);
    out
}

fn main() {
    let t = Instant::now();
    let r = pi_digits(30);
    let ok = r == "3.1415926535897932384626433832\n";
    println!("pi {} {}", t.elapsed().as_millis(), if ok { "ok" } else { "WRONG" });
}
