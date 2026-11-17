// The C# benchmarks, one class each with Run(), chosen by name: `Bench fib`. Each times its own work and prints the
// line the xBase benchmarks print: `<name> <milliseconds> ok|WRONG`.
using System;
using System.Diagnostics;
using System.Threading.Tasks;

static class Program {
  public static void Report(string name, Stopwatch sw, bool ok) =>
    Console.WriteLine($"{name} {sw.ElapsedMilliseconds} {(ok ? "ok" : "WRONG")}");

  static async Task Main(string[] args) {
    switch (args.Length > 0 ? args[0] : "") {
    case "fib": Fib.Run(); break;
    case "loops": Loops.Run(); break;
    case "sieve": Sieve.Run(); break;
    case "matmul": MatMul.Run(); break;
    case "qsort": QSortBench.Run(); break;
    case "asort": ASortBench.Run(); break;
    case "strings": Strings.Run(); break;
    case "crc32": Crc32.Run(); break;
    case "float": FloatBench.Run(); break;
    case "mixed": Mixed.Run(); break;
    case "oop": Oop.Run(); break;
    case "gc": Gc.Run(); break;
    case "bitlookup": BitLookup.Run(); break;
    case "generator": Generator.Run(); break;
    case "pi": Pi.Run(); break;
    case "tasks": await TasksBench.Run(); break;
    case "channels": await ChannelsBench.Run(); break;
    case "async": await AsyncBench.Run(); break;
    case "async_pi": await AsyncPi.Run(); break;
    default: Console.Error.WriteLine("usage: Bench <name>"); Environment.Exit(2); break;
    }
  }
}
