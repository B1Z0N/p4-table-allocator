import json
import pathlib
import subprocess
import sys

root = pathlib.Path(__file__).resolve().parents[1]
allocator = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "build/allocator"
fits = {"greedy": 0, "constrained": 0}
examples = sorted((root / "examples").glob("*.json"))

print("| Example | Greedy | Constrained |")
print("| --- | ---: | ---: |")
for example in examples:
    process = subprocess.run([str(allocator), "--compare", "--json", str(example)],
                             capture_output=True, text=True)
    if process.returncode not in (0, 2):
        raise SystemExit(f"{example.name}: {process.stderr or process.stdout}")
    report = json.loads(process.stdout)
    stages = []
    for algorithm in ("greedy", "constrained"):
        result = report[algorithm]
        fits[algorithm] += int(result["fits"])
        stages.append(str(result["stages_used"]) if result["fits"] else "—")
    print(f"| {example.stem} | {' | '.join(stages)} |")

print(f"\nGreedy fits {fits['greedy']} of the {len(examples)} examples; "
      f"constrained fits {fits['constrained']}.")
