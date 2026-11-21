#!/usr/bin/env bash
# Builds each benchmark with xbase and with every other compiler or interpreter found (bench/<language>/: the same
# work, sizes and checks), runs each RUNS times and prints a Markdown table of the best time of each, in milliseconds.
# xbase uses a Release build (BUILD, configured when missing); Harbour compiles the .prg itself (bench/compat.ch, its
# class syntax), or its own form in bench/harbour/, with its fastest C output (-gc3; -gc2 where HARBOUR_GC2 says) and
# the same optimization as xbc's (-O2). A cell is "WRONG" when the program's check failed, "fail" when it did not
# build or run, "timeout" past LIMIT seconds, "-" when that language has no version.
# The programs run in BUILD/compare/run (the index benchmark writes its files there).
#
#   bench/compare.sh [-n RUNS] [-b BUILD] [-l LANGS] [-t LIMIT] [bench ...]
#
# LANGS is a comma-separated list among xbase,harbour,c,rust,csharp,java,go,node,pypy,python (default: all found).
# CPython (python) runs each benchmark once whatever RUNS is: it is slow. Needs bash 4 and GNU timeout (gtimeout on
# macOS, from coreutils).
set -u
if [ "${BASH_VERSINFO[0]}" -lt 4 ]; then
  echo "compare.sh needs bash 4 or newer (or use bench/compare.ps1)" >&2
  exit 2
fi
TIMEOUT=$(command -v timeout || command -v gtimeout) || { echo "compare.sh needs GNU timeout" >&2; exit 2; }
cd "$(dirname "$0")/.."
ROOT=$PWD
RUNS=3
BUILD=build-release
LIMIT=300
LANGS=
while getopts "n:b:l:t:" opt; do
  case $opt in
    n) RUNS=$OPTARG ;;
    b) BUILD=$OPTARG ;;
    l) LANGS=$OPTARG ;;
    t) LIMIT=$OPTARG ;;
    *) sed -n '2,14p' "$0"; exit 2 ;;
  esac
done
shift $((OPTIND - 1))
BENCHES=("$@")
if [ ${#BENCHES[@]} -eq 0 ]; then
  for f in bench/*.prg; do BENCHES+=("$(basename "$f" .prg)"); done
fi
mkdir -p "$BUILD/compare/run"
BUILD=$(cd "$BUILD" && pwd)
OUT=$BUILD/compare

ALL=(xbase harbour c rust csharp java go node pypy python)
declare -A TOOL=([xbase]=cmake [harbour]=hbmk2 [c]=gcc [rust]=rustc [csharp]=dotnet [java]=javac [go]=go
                 [node]=node [pypy]=pypy3 [python]=python3)
declare -A TITLE=([xbase]=xbase [harbour]=Harbour [c]=C [rust]=Rust [csharp]=C# [java]=Java [go]=Go
                  [node]=JavaScript [pypy]="Python (PyPy)" [python]="Python (CPython)")
# What Harbour has no counterpart of (YIELD, a pool of tasks, bounded channels, ASYNC, lazy enumerables, atomics,
# memory statistics); those of its own forms in bench/harbour/ that run threads, built with its threaded runtime.
HARBOUR_SKIP=" generator tasks channels async async_generator higher_order_chain pipeline_tasks work_stealing 
              shared_counter memory_footprint "
HARBOUR_MT=" async_pi parallel_map producer_consumer chameneos_redux "
# Its -gc3 output multiplies by an integer constant in doubles, rounding products past 2^53, which these programs'
# generators (seed * 1103515245) need exact: they are built with -gc2, which multiplies them in 64-bit integers.
HARBOUR_GC2=" black_scholes crc_adler_fnv dgemm fft md5_sha1 monte_carlo_pi radix_sort random_massivo "
declare -A JAVA=([fib]=Fib [loops]=Loops [sieve]=Sieve [matmul]=MatMul [qsort]=QSort [asort]=ASort [strings]=Strings
                 [crc32]=Crc32 [float]=FloatBench [mixed]=Mixed [oop]=Oop [gc]=Gc [bitlookup]=BitLookup
                 [generator]=Generator [pi]=Pi [tasks]=Tasks [channels]=Channels [async_pi]=AsyncPi
                 [ackermann]=Ackermann [aeval_heval]=AevalHeval [array_append]=ArrayAppend [array_growth]=ArrayGrowth
                 [array_nested]=ArrayNested [array_of_hashes]=ArrayOfHashes [array_preallocate]=ArrayPreallocate
                 [asort_codeblock]=AsortCodeblock [at_rat_strtran]=AtRatStrtran [base64]=Base64 [bfs_dfs]=BfsDfs
                 [binary_search]=BinarySearch [binary_trees]=BinaryTrees [black_scholes]=BlackScholes
                 [chameneos_redux]=ChameneosRedux [closure_capture]=ClosureCapture
                 [codeblock_callback]=CodeblockCallback [codeblock_eval]=CodeblockEval [counting_sort]=CountingSort
                 [crc_adler_fnv]=CrcAdlerFnv [csv_parse]=CsvParse [date_time_ops]=DateTimeOps [dgemm]=Dgemm
                 [dhrystone]=Dhrystone [dijkstra]=Dijkstra [fannkuch_redux]=FannkuchRedux [fasta]=Fasta [fft]=Fft
                 [gaussian_elimination]=GaussianElimination [geracao_html]=GeracaoHtml
                 [hash_chaves_longas]=HashChavesLongas [hash_massivo]=HashMassivo [hash_resize]=HashResize
                 [heap_sort]=HeapSort [higher_order_chain]=HigherOrderChain [indexacao_simulada]=IndexacaoSimulada
                 [index]=Index [inheritance_depth]=InheritanceDepth [ini_parse]=IniParse [jaro_winkler]=JaroWinkler
                 [join_arrays]=JoinArrays [json_roundtrip]=JsonRoundtrip [julia_set]=JuliaSet [knapsack]=Knapsack
                 [k_nucleotide]=KNucleotide [levenshtein]=Levenshtein [linked_list]=LinkedList [linpack]=Linpack
                 [log_processing]=LogProcessing [longest_common_subsequence]=LongestCommonSubsequence
                 [long_short_mix]=LongShortMix [lru_cache]=LruCache [mandelbrot]=Mandelbrot
                 [map_filter_reduce]=MapFilterReduce [md5_sha1]=Md5Sha1 [memory_footprint]=MemoryFootprint
                 [merge_sort]=MergeSort [meteor_contest]=MeteorContest [method_call_overhead]=MethodCallOverhead
                 [monte_carlo_pi]=MonteCarloPi [n_body]=NBody [n_queens]=NQueens
                 [numerical_integration]=NumericalIntegration [numeric_formatting]=NumericFormatting
                 [object_churn]=ObjectChurn [object_creation]=ObjectCreation [parallel_map]=ParallelMap
                 [pidigits]=Pidigits [pivot_crosstab]=PivotCrosstab [prime_sieve_atkin]=PrimeSieveAtkin
                 [producer_consumer]=ProducerConsumer [property_access]=PropertyAccess [radix_sort]=RadixSort
                 [random_massivo]=RandomMassivo [ray_tracer]=RayTracer [relatorio_agregacao]=RelatorioAgregacao
                 [reverse_complement]=ReverseComplement [sequential_scan]=SequentialScan [set_operations]=SetOperations
                 [shared_counter]=SharedCounter [short_lived_objects]=ShortLivedObjects [spectral_norm]=SpectralNorm
                 [string_builder]=StringBuilderBench [string_concatenation]=StringConcatenation
                 [substr_massivo]=SubstrMassivo [sudoku_solver]=SudokuSolver [tak]=Tak [tokenize_split]=TokenizeSplit
                 [towers_of_hanoi]=TowersOfHanoi [trie]=Trie [upper_lower_alltrim]=UpperLowerAllTrim
                 [url_encode]=UrlEncode [validacao_dados]=ValidacaoDados [virtual_dispatch]=VirtualDispatch
                 [whetstone]=Whetstone [word_count]=WordCount [work_stealing]=WorkStealing [xml_simples]=XmlSimples)
# The Python ports named otherwise than their benchmark: a standard library module's name would hide that module.
declare -A PYTHON=([base64]=base64_bench)
declare -A BUILT=() # per language, whether its shared build (prepare) worked

USE=()
for l in "${ALL[@]}"; do
  if [ -n "$LANGS" ] && [[ ",$LANGS," != *",$l,"* ]]; then continue; fi
  command -v "${TOOL[$l]}" >/dev/null 2>&1 && USE+=("$l")
done

# Builds what language $1 needs once for all its benchmarks.
prepare() {
  case $1 in
    xbase)
      if [ ! -f "$BUILD/CMakeCache.txt" ]; then
        cmake -S . -B "$BUILD" -DCMAKE_BUILD_TYPE=Release >/dev/null || return 1
      elif ! grep -q "^CMAKE_BUILD_TYPE:STRING=Release$" "$BUILD/CMakeCache.txt"; then
        echo "$BUILD is not a Release build: xbase is measured from Release builds only" >&2
        return 1
      fi ;;
    csharp) # the SDK's collector reserves 8 GB of addresses, past a limit the script may run under: 4 GB
      env DOTNET_GCRegionRange=0x100000000 DOTNET_GCHeapHardLimit=0x100000000 \
        dotnet build bench/csharp -c Release -v q -nologo -nodeReuse:false -p:UseSharedCompilation=false \
        -o "$OUT/csharp" -p:BaseIntermediateOutputPath="$OUT/csharp-obj/" >/dev/null 2>&1 ;;
    java)
      mkdir -p "$OUT/java" && javac -d "$OUT/java" bench/java/*.java >/dev/null 2>&1 ;;
  esac
}

# The command that runs benchmark $2 in language $1, built first when it is compiled; "fail" when its build failed;
# nothing when the language has no version of it.
command_of() {
  local l=$1 b=$2 exe
  case $l in
    xbase) # a single-configuration build, or the Release one of a multi-configuration build
      rm -f "$BUILD/bench/bench_$b" "$BUILD/bench/Release/bench_$b"
      if cmake --build "$BUILD" -j4 --config Release --target "bench_$b" >/dev/null 2>&1; then
        for exe in "$BUILD/bench/bench_$b" "$BUILD/bench/Release/bench_$b"; do
          [ -x "$exe" ] && { echo "$exe"; return; }
        done
      fi ;;
    harbour)
      [[ $HARBOUR_SKIP == *" $b "* ]] && return
      exe=$OUT/harbour/$b
      src=$ROOT/bench/$b.prg
      [ -f "$ROOT/bench/harbour/$b.prg" ] && src=$ROOT/bench/harbour/$b.prg
      mt=()
      [[ $HARBOUR_MT == *" $b "* ]] && mt=(-mt)
      gc=-gc3
      [[ $HARBOUR_GC2 == *" $b "* ]] && gc=-gc2
      mkdir -p "$OUT/harbour" && (cd "$OUT/harbour" && rm -f "$b" &&
        hbmk2 "${mt[@]}" -static -gtcgi -q -w0 "$gc" -cflag=-O2 -u+hbclass.ch -u+"$ROOT/bench/compat.ch" hbct.hbc \
          "$src" -o"$b" >/dev/null 2>&1) && { echo "$exe"; return; } ;;
    c)
      [ -f "bench/c/$b.c" ] || return
      gcc -O2 -o "$OUT/c-$b" "bench/c/$b.c" -lm -lpthread 2>/dev/null && { echo "$OUT/c-$b"; return; } ;;
    rust)
      [ -f "bench/rust/$b.rs" ] || return
      rustc -C opt-level=2 -o "$OUT/rust-$b" "bench/rust/$b.rs" 2>/dev/null && { echo "$OUT/rust-$b"; return; } ;;
    csharp)
      grep -q "case \"$b\"" bench/csharp/Program.cs || return
      # its collector reserves 8 GB of address space by default, past the run's limit (measure): a 4 GB heap
      [ -n "${BUILT[csharp]}" ] && { echo "env DOTNET_GCHeapHardLimit=0x100000000 $OUT/csharp/Bench $b"; return; } ;;
    java)
      [ -n "${JAVA[$b]:-}" ] || return
      [ -n "${BUILT[java]}" ] && { echo "java -cp $OUT/java ${JAVA[$b]}"; return; } ;;
    go)
      [ -f "bench/go/$b.go" ] || return
      go build -o "$OUT/go-$b" "bench/go/$b.go" 2>/dev/null && { echo "$OUT/go-$b"; return; } ;;
    node) [ -f "bench/js/$b.js" ] && echo "node $ROOT/bench/js/$b.js"; return ;;
    pypy | python)
      local py=bench/python/${PYTHON[$b]:-$b}.py tool=pypy3
      [ "$l" = python ] && tool=python3
      [ -f "$py" ] && echo "$tool -B $ROOT/$py"
      return ;;
  esac
  echo fail
}

# The best time of $2 runs of command $1, or WRONG, fail or timeout.
measure() {
  local cmd=$1 runs=$2 best= rc line
  for _ in $(seq 1 "$runs"); do
    # shellcheck disable=SC2086 # the command's words are split on purpose
    (cd "$OUT/run" && ulimit -v 16000000 && "$TIMEOUT" "$LIMIT" $cmd >"$OUT/run.txt" 2>/dev/null)
    rc=$?
    [ $rc -eq 124 ] && { echo timeout; return; }
    line=$(tail -1 "$OUT/run.txt")
    # shellcheck disable=SC2086
    set -- $line
    [ $rc -eq 0 ] && [ $# -ge 3 ] || { echo fail; return; }
    [ "$3" = ok ] || { echo WRONG; return; }
    if [ -z "$best" ] || [ "$2" -lt "$best" ]; then best=$2; fi
  done
  echo "$best"
}

for l in "${USE[@]}"; do
  if prepare "$l"; then BUILT[$l]=1; else BUILT[$l]=; fi
done
header="| benchmark |"
rule="|---|"
for l in "${USE[@]}"; do
  header+=" ${TITLE[$l]} |"
  rule+="---:|"
done
{
  echo "$header"
  echo "$rule"
  for b in "${BENCHES[@]}"; do
    row="| $b |"
    for l in "${USE[@]}"; do
      if [ "$l" = xbase ] && [ -z "${BUILT[xbase]}" ]; then cmd=fail; else cmd=$(command_of "$l" "$b"); fi
      if [ -z "$cmd" ]; then
        row+=" - |"
      elif [ "$cmd" = fail ]; then
        row+=" fail |"
      else
        runs=$RUNS
        [ "$l" = python ] && runs=1
        row+=" $(measure "$cmd" "$runs") |"
      fi
    done
    echo "$row"
  done
} | tee "$OUT/compare.md"
rm -f "$OUT/run.txt"
