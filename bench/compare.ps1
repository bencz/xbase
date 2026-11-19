#Requires -Version 7.2
# Builds each benchmark with xbase and with every other compiler or interpreter found (bench/<language>/: the same
# work, sizes and checks), runs each -Runs times and prints a Markdown table of the best time of each, in milliseconds.
# The same as bench/compare.sh: xbase uses a Release build (-Build, configured when missing); Harbour compiles the
# .prg itself (bench/compat.ch, its class syntax), or its own form in bench/harbour/, with its fastest C output and
# -O2. A cell is "WRONG" when the program's check failed, "fail" when it did not build or run, "timeout" past -Limit
# seconds, "-" when that language has no version. The programs run in <Build>/compare/run (the index benchmark writes
# its files there).
#
#   bench/compare.ps1 [-Runs 3] [-Build build-release] [-Langs xbase,c,...] [-Limit 300] [bench ...]
#
# -Langs takes languages among xbase, harbour, c, rust, csharp, java, go, node, pypy, python (default: all found).
# CPython (python) runs each benchmark once whatever -Runs is: it is slow.
param(
  [int]$Runs = 3,
  [string]$Build = "build-release",
  [string[]]$Langs = @(),
  [int]$Limit = 300,
  [Parameter(ValueFromRemainingArguments = $true)][string[]]$Benches
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
if (-not $Benches) { $Benches = Get-ChildItem bench/*.prg | ForEach-Object { $_.BaseName } }
New-Item -ItemType Directory -Force -Path (Join-Path $Build "compare/run") | Out-Null
$Build = (Resolve-Path $Build).Path
$Out = Join-Path $Build "compare"
$RunDir = Join-Path $Out "run"
$Exe = if ($IsWindows) { ".exe" } else { "" }
$Wanted = @($Langs | ForEach-Object { $_ -split "," } | Where-Object { $_ })

$All = "xbase", "harbour", "c", "rust", "csharp", "java", "go", "node", "pypy", "python"
$Tool = @{ xbase = "cmake"; harbour = "hbmk2"; c = "gcc"; rust = "rustc"; csharp = "dotnet"; java = "javac";
           go = "go"; node = "node"; pypy = "pypy3"; python = "python3" }
$Title = @{ xbase = "xbase"; harbour = "Harbour"; c = "C"; rust = "Rust"; csharp = "C#"; java = "Java"; go = "Go";
            node = "JavaScript"; pypy = "Python (PyPy)"; python = "Python (CPython)" }
# What Harbour has no counterpart of (YIELD, a pool of tasks, bounded channels, ASYNC); those of its own forms in
# bench/harbour/ that run threads, built with its threaded runtime.
$HarbourSkip = "generator", "tasks", "channels", "async", "async_generator", "higher_order_chain", "pipeline_tasks",
               "work_stealing", "shared_counter", "memory_footprint"
$HarbourMt = "async_pi", "parallel_map", "producer_consumer", "chameneos_redux"
$Java = @{ fib = "Fib"; loops = "Loops"; sieve = "Sieve"; matmul = "MatMul"; qsort = "QSort"; asort = "ASort";
          strings = "Strings"; crc32 = "Crc32"; float = "FloatBench"; mixed = "Mixed"; oop = "Oop"; gc = "Gc";
          bitlookup = "BitLookup"; generator = "Generator"; pi = "Pi"; tasks = "Tasks"; channels = "Channels";
          async_pi = "AsyncPi"; ackermann = "Ackermann"; aeval_heval = "AevalHeval"; array_append = "ArrayAppend";
          array_growth = "ArrayGrowth"; array_nested = "ArrayNested"; array_of_hashes = "ArrayOfHashes";
          array_preallocate = "ArrayPreallocate"; asort_codeblock = "AsortCodeblock"; at_rat_strtran = "AtRatStrtran";
          base64 = "Base64"; bfs_dfs = "BfsDfs"; binary_search = "BinarySearch"; binary_trees = "BinaryTrees";
          black_scholes = "BlackScholes"; chameneos_redux = "ChameneosRedux"; closure_capture = "ClosureCapture";
          codeblock_callback = "CodeblockCallback"; codeblock_eval = "CodeblockEval"; counting_sort = "CountingSort";
          crc_adler_fnv = "CrcAdlerFnv"; csv_parse = "CsvParse"; date_time_ops = "DateTimeOps"; dgemm = "Dgemm";
          dhrystone = "Dhrystone"; dijkstra = "Dijkstra"; fannkuch_redux = "FannkuchRedux"; fasta = "Fasta";
          fft = "Fft"; gaussian_elimination = "GaussianElimination"; geracao_html = "GeracaoHtml";
          hash_chaves_longas = "HashChavesLongas"; hash_massivo = "HashMassivo"; hash_resize = "HashResize";
          heap_sort = "HeapSort"; higher_order_chain = "HigherOrderChain"; indexacao_simulada = "IndexacaoSimulada";
          index = "Index"; inheritance_depth = "InheritanceDepth"; ini_parse = "IniParse";
          jaro_winkler = "JaroWinkler"; join_arrays = "JoinArrays"; json_roundtrip = "JsonRoundtrip";
          julia_set = "JuliaSet"; knapsack = "Knapsack"; k_nucleotide = "KNucleotide"; levenshtein = "Levenshtein";
          linked_list = "LinkedList"; linpack = "Linpack"; log_processing = "LogProcessing";
          longest_common_subsequence = "LongestCommonSubsequence"; long_short_mix = "LongShortMix";
          lru_cache = "LruCache"; mandelbrot = "Mandelbrot"; map_filter_reduce = "MapFilterReduce";
          md5_sha1 = "Md5Sha1"; memory_footprint = "MemoryFootprint"; merge_sort = "MergeSort";
          meteor_contest = "MeteorContest"; method_call_overhead = "MethodCallOverhead";
          monte_carlo_pi = "MonteCarloPi"; n_body = "NBody"; n_queens = "NQueens";
          numerical_integration = "NumericalIntegration"; numeric_formatting = "NumericFormatting";
          object_churn = "ObjectChurn"; object_creation = "ObjectCreation"; parallel_map = "ParallelMap";
          pidigits = "Pidigits"; pivot_crosstab = "PivotCrosstab"; prime_sieve_atkin = "PrimeSieveAtkin";
          producer_consumer = "ProducerConsumer"; property_access = "PropertyAccess"; radix_sort = "RadixSort";
          random_massivo = "RandomMassivo"; ray_tracer = "RayTracer"; relatorio_agregacao = "RelatorioAgregacao";
          reverse_complement = "ReverseComplement"; sequential_scan = "SequentialScan";
          set_operations = "SetOperations"; shared_counter = "SharedCounter";
          short_lived_objects = "ShortLivedObjects"; spectral_norm = "SpectralNorm";
          string_builder = "StringBuilderBench"; string_concatenation = "StringConcatenation";
          substr_massivo = "SubstrMassivo"; sudoku_solver = "SudokuSolver"; tak = "Tak";
          tokenize_split = "TokenizeSplit"; towers_of_hanoi = "TowersOfHanoi"; trie = "Trie";
          upper_lower_alltrim = "UpperLowerAllTrim"; url_encode = "UrlEncode"; validacao_dados = "ValidacaoDados";
          virtual_dispatch = "VirtualDispatch"; whetstone = "Whetstone"; word_count = "WordCount";
          work_stealing = "WorkStealing"; xml_simples = "XmlSimples" }
# The Python ports named otherwise than their benchmark: a standard library module's name would hide that module.
$Python = @{ base64 = "base64_bench" }
$Built = @{}

$Use = $All | Where-Object {
  ($Wanted.Count -eq 0 -or $Wanted -contains $_) -and (Get-Command $Tool[$_] -ErrorAction SilentlyContinue)
}

# Builds what language $l needs once for all its benchmarks: $true when it worked.
function Prepare([string]$l) {
  switch ($l) {
    "xbase" {
      $cache = Join-Path $Build "CMakeCache.txt"
      if (-not (Test-Path $cache)) {
        cmake -S . -B $Build -DCMAKE_BUILD_TYPE=Release 2>&1 | Out-Null
        return $LASTEXITCODE -eq 0
      }
      $type = Select-String -Path $cache -Pattern "^CMAKE_BUILD_TYPE:STRING=(.*)$"
      $multi = Select-String -Quiet -Path $cache -Pattern "^CMAKE_CONFIGURATION_TYPES:"
      if (-not $multi -and (-not $type -or $type.Matches[0].Groups[1].Value -ne "Release")) {
        Write-Error "$Build is not a Release build: xbase is measured from Release builds only" -ErrorAction Continue
        return $false
      }
      return $true
    }
    "csharp" {
      dotnet build bench/csharp -c Release -v q -nologo -nodeReuse:false -p:UseSharedCompilation=false `
        -o (Join-Path $Out "csharp") "-p:BaseIntermediateOutputPath=$(Join-Path $Out 'csharp-obj')/" 2>&1 | Out-Null
      return $LASTEXITCODE -eq 0
    }
    "java" {
      javac -d (Join-Path $Out "java") (Get-ChildItem bench/java/*.java) 2>&1 | Out-Null
      return $LASTEXITCODE -eq 0
    }
  }
  return $true
}

# The program and arguments that run benchmark $b in language $l, built first when it is compiled; "fail" when its
# build failed; $null when the language has no version of it.
function CommandOf([string]$l, [string]$b) {
  switch ($l) {
    "xbase" {
      if (-not $Built.xbase) { return "fail" }
      # A single-configuration build, or the Release one of a multi-configuration build.
      $exes = (Join-Path $Build "bench/bench_$b$Exe"), (Join-Path $Build "bench/Release/bench_$b$Exe")
      Remove-Item -ErrorAction SilentlyContinue $exes
      cmake --build $Build -j4 --config Release --target "bench_$b" 2>&1 | Out-Null
      if ($LASTEXITCODE -eq 0) {
        foreach ($e in $exes) { if (Test-Path $e) { return , @($e) } }
      }
      return "fail"
    }
    "harbour" {
      if ($HarbourSkip -contains $b) { return $null }
      $dir = Join-Path $Out "harbour"
      New-Item -ItemType Directory -Force -Path $dir | Out-Null
      Remove-Item -ErrorAction SilentlyContinue (Join-Path $dir "$b$Exe")
      Push-Location $dir
      $src = Join-Path $Root "bench/harbour/$b.prg"
      if (-not (Test-Path $src)) { $src = Join-Path $Root "bench/$b.prg" }
      $mt = @()
      if ($HarbourMt -contains $b) { $mt = @("-mt") }
      try {
        hbmk2 @mt -static -gtcgi -q -w0 -gc3 -cflag=-O2 "-u+hbclass.ch" "-u+$Root/bench/compat.ch" $src "-o$b" 2>&1 |
          Out-Null
      } finally { Pop-Location }
      if ($LASTEXITCODE -eq 0) { return , @(Join-Path $dir "$b$Exe") }
      return "fail"
    }
    "c" {
      if (-not (Test-Path "bench/c/$b.c")) { return $null }
      $e = Join-Path $Out "c-$b$Exe"
      gcc -O2 -o $e "bench/c/$b.c" -lm -lpthread 2>&1 | Out-Null
      if ($LASTEXITCODE -eq 0) { return , @($e) }
      return "fail"
    }
    "rust" {
      if (-not (Test-Path "bench/rust/$b.rs")) { return $null }
      $e = Join-Path $Out "rust-$b$Exe"
      rustc -C opt-level=2 -o $e "bench/rust/$b.rs" 2>&1 | Out-Null
      if ($LASTEXITCODE -eq 0) { return , @($e) }
      return "fail"
    }
    "csharp" {
      if (-not (Select-String -Quiet -Path bench/csharp/Program.cs -Pattern "case `"$b`"")) { return $null }
      if (-not $Built.csharp) { return "fail" }
      return , @((Join-Path $Out "csharp/Bench$Exe"), $b)
    }
    "java" {
      if (-not $Java.ContainsKey($b)) { return $null }
      if (-not $Built.java) { return "fail" }
      return , @("java", "-cp", (Join-Path $Out "java"), $Java[$b])
    }
    "go" {
      if (-not (Test-Path "bench/go/$b.go")) { return $null }
      $e = Join-Path $Out "go-$b$Exe"
      go build -o $e "bench/go/$b.go" 2>&1 | Out-Null
      if ($LASTEXITCODE -eq 0) { return , @($e) }
      return "fail"
    }
    "node" { if (Test-Path "bench/js/$b.js") { return , @("node", (Join-Path $Root "bench/js/$b.js")) } }
    { $_ -eq "pypy" -or $_ -eq "python" } {
      $py = "bench/python/$(if ($Python.ContainsKey($b)) { $Python[$b] } else { $b }).py"
      if (Test-Path $py) { return , @($Tool[$l], "-B", (Join-Path $Root $py)) }
    }
  }
  return $null
}

# The best time of $runs runs of a command, or WRONG, fail or timeout.
function Measure-Bench([string[]]$cmd, [int]$runs) {
  $best = $null
  $outFile = Join-Path $Out "run.txt"
  for ($k = 0; $k -lt $runs; $k++) {
    $start = @{ FilePath = $cmd[0]; NoNewWindow = $true; PassThru = $true; WorkingDirectory = $RunDir;
                RedirectStandardOutput = $outFile; RedirectStandardError = (Join-Path $Out "run.err") }
    if ($cmd.Length -gt 1) { $start.ArgumentList = $cmd[1..($cmd.Length - 1)] }
    try { $p = Start-Process @start } catch { return "fail" }
    $null = $p.Handle # keeps the process handle, so that ExitCode is known after the wait
    if (-not $p.WaitForExit($Limit * 1000)) {
      $p.Kill($true)
      $p.WaitForExit()
      return "timeout"
    }
    $p.WaitForExit()
    $line = Get-Content $outFile -ErrorAction SilentlyContinue | Select-Object -Last 1
    $w = if ($line) { @($line -split "\s+" | Where-Object { $_ }) } else { @() }
    if ($p.ExitCode -ne 0 -or $w.Count -lt 3) { return "fail" }
    if ($w[2] -ne "ok") { return "WRONG" }
    $ms = [int64]$w[1]
    if ($null -eq $best -or $ms -lt $best) { $best = $ms }
  }
  return "$best"
}

foreach ($l in $Use) { $Built[$l] = Prepare $l }
$lines = @()
$lines += "| benchmark |" + (($Use | ForEach-Object { " $($Title[$_]) |" }) -join "")
$lines += "|---|" + (($Use | ForEach-Object { "---:|" }) -join "")
$lines | Write-Output
foreach ($b in $Benches) {
  $row = "| $b |"
  foreach ($l in $Use) {
    $cmd = CommandOf $l $b
    if (-not $cmd) { $row += " - |"; continue }
    if ($cmd -is [string]) { $row += " $cmd |"; continue }
    $n = if ($l -eq "python") { 1 } else { $Runs }
    $row += " $(Measure-Bench $cmd $n) |"
  }
  $lines += $row
  Write-Output $row
}
$lines | Set-Content (Join-Path $Out "compare.md")
Remove-Item -ErrorAction SilentlyContinue (Join-Path $Out "run.txt"), (Join-Path $Out "run.err")
