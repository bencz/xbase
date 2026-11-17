// Shared by the JavaScript benchmarks: the result line the xBase benchmarks print, `<name> <milliseconds> ok|WRONG`.
exports.report = (name, start, ok) =>
  console.log(`${name} ${Math.round(performance.now() - start)} ${ok ? "ok" : "WRONG"}`);
