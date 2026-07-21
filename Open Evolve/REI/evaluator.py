import sys
import uuid
import pathlib
import traceback
import subprocess
import json
import re
import shutil
from datetime import datetime

BASE_DIR = pathlib.Path(__file__).parent.resolve()
# To import openevolve which is in root
sys.path.append(str(BASE_DIR.parent))

from openevolve.evaluation_result import EvaluationResult
from code_to_query import parse_output_code, save_parsed_output_code

PROJECT_TEMP_DIRECTORY = BASE_DIR / "temp"
BENCHMARKS_JSON = BASE_DIR / "benchmarks.json"

BASE_SOLVE_REWARD = 1
RANGE_WEIGHT = 10
COST_WEIGHT = 100
RES_WEIGHT = 100

def get_weight(benchmark_path):
    if "16-127" in benchmark_path: return 1
    if "128-255" in benchmark_path: return 2
    if "256-511" in benchmark_path: return 4
    if "512-1023" in benchmark_path: return 8
    if "1024-2047" in benchmark_path: return 16
    return 1

def evaluate(program_path: str) -> EvaluationResult:
    solution_dir_id = datetime.now().strftime("%Y_%m_%d-%H_%M_%S") + '-' + str(uuid.uuid4())
    solution_dir_path = PROJECT_TEMP_DIRECTORY / solution_dir_id

    try:
        # Parse and save code
        with open(program_path, 'r', encoding='utf-8-sig') as f:
            code_text = f.read()
        
        parsed_code = parse_output_code(code_text)
        if not parsed_code:
            return EvaluationResult(metrics={"combined_score": 0.0}, artifacts={"error": "Failed to parse program"})

        original_code_dir = BASE_DIR / "code"
        shutil.copytree(original_code_dir, solution_dir_path, dirs_exist_ok=True, ignore=shutil.ignore_patterns('build', '.cache', 'out'))
        save_parsed_output_code(parsed_code, str(solution_dir_path))
        
        metrics = {
            "combined_score": 0.0,
            "solved_count": 0,
            "compilation_success": 0.0
        }
        artifacts = {}
        
        # Run pre-computation script
        stats_script = solution_dir_path / "stats_extractor.py"
        if stats_script.exists():
            print(f"Running {stats_script.name}...")
            # We pass the absolute path to benchmark_results.json
            json_path = BASE_DIR / "benchmark_results.json"
            subprocess.run(["python", str(stats_script), str(json_path)], cwd=str(solution_dir_path))

        # Compile
        build_dir = solution_dir_path / "build"
        build_dir.mkdir(parents=True, exist_ok=True)
        print(f"Compiling CMake project in {build_dir}...")
        compile_cmd1 = f'cmake -B "{build_dir}" -S "{solution_dir_path}" -DBUILD_BIDIRECTIONAL=ON'
        compile_cmd2 = f'cmake --build "{build_dir}" --config Release'
        
        c1 = subprocess.run(compile_cmd1, shell=True, capture_output=True, text=True)
        if c1.returncode != 0:
            with open(BASE_DIR / "cmake_error.log", "w") as f: f.write(c1.stderr)
            print("CMake Configure Failed:\n", c1.stderr)
            return EvaluationResult(metrics=metrics, artifacts={"error": "CMake Configure Failed", "stderr": c1.stderr})
            
        c2 = subprocess.run(compile_cmd2, shell=True, capture_output=True, text=True)
        if c2.returncode != 0:
            with open(BASE_DIR / "cmake_error.log", "w") as f: f.write(c2.stdout)
            print("CMake Build Failed:\n", c2.stdout)
            return EvaluationResult(metrics=metrics, artifacts={"error": "CMake Build Failed", "stdout": c2.stdout})
            
        metrics["compilation_success"] = 1.0
        
        executable_path = build_dir / "bidirectional" / "Release" / "REI.exe"
        if not executable_path.exists():
            executable_path = build_dir / "Release" / "REI.exe"
            
        if not executable_path.exists():
            executable_path = build_dir / "REI.exe"
            
        if not executable_path.exists():
            print(f"ERROR: Executable not found at {build_dir / 'bidirectional' / 'Release' / 'REI.exe'} or RegexInference.exe")
            return EvaluationResult(metrics=metrics, artifacts={"error": "Executable not found"})

        # Load benchmarks
        with open(BENCHMARKS_JSON, 'r') as f:
            benchmarks = json.load(f)
            
        total_score = 0.0
        solved = 0
        max_cost = 500
        
        # Run benchmarks
        print(f"Running {len(benchmarks)} benchmarks...")
        for bench in benchmarks:
            b_path = bench["path"]
            target_cost = bench["Cost"]
            weight = get_weight(b_path)
            
            target_res = 1
            
            abs_b_path = BASE_DIR / b_path
            if not abs_b_path.exists():
                continue
                
            run_cmd = [str(executable_path), str(abs_b_path), str(max_cost), "1", "1", "1", "1", "1"]
            print(f"  Benchmark {b_path} started.")
            try:
                # 60 seconds timeout per benchmark
                r = subprocess.run(run_cmd, capture_output=True, text=True, timeout=60)
                print(f"  Benchmark {b_path} finished.")
                output = r.stdout
                
                # Check correctness
                if "regex didn't match" in output or "regex did match" in output:
                    continue
                    
                if "==== Output ===" in output:
                    final_output = output.split("==== Output ===")[-1]
                else:
                    final_output = output
                    
                cost_match = re.search(r"Cost:\s+(\d+)", final_output)
                res_match = re.search(r"(?:All\s+)?REs:\s+(\d+)", final_output)
                if cost_match:
                    actual_cost = int(cost_match.group(1))
                    actual_res = int(res_match.group(1)) if res_match else 0
                    
                    if actual_res > 0:
                        solved += BASE_SOLVE_REWARD

                        # Give points for solving it
                        score = weight * RANGE_WEIGHT 
                        
                        # Bonus for Cost
                        if actual_cost <= target_cost:
                            score += weight * RANGE_WEIGHT
                        else:
                            score += weight * COST_WEIGHT * (target_cost / actual_cost)
                            
                        # Penalty for high REs searched
                        if actual_res:
                            score += weight * RES_WEIGHT * (target_res / actual_res)
                                
                        total_score += score
                        print(f"  Benchmark {bench['path']} completed. Cost: {actual_cost}/{target_cost}, REs: {actual_res}/{target_res}, Score added: {score}")
                    else:
                        print(f"  Benchmark {bench['path']} failed to find REs. Cost: {actual_cost}/{target_cost}, REs: {actual_res}/{target_res}")
                else:
                    print(f"  Benchmark {bench['path']} completed without a valid Cost. Output: {output.strip()}")
            except subprocess.TimeoutExpired:
                print(f"  Benchmark {bench['path']} timed out.")
                continue
            except Exception as e:
                print(f"  Benchmark {bench['path']} failed with exception: {e}")
                continue

        metrics["combined_score"] = total_score
        metrics["solved_count"] = solved
        
    except Exception as e:
        print(f"Evaluation Exception: {e}\n{traceback.format_exc()}")
        artifacts = {
            "error": str(e),
            "traceback": traceback.format_exc()
        }
        metrics = {"combined_score": 0.0}
    finally:
        # Cleanup
        try:
            shutil.rmtree(solution_dir_path, ignore_errors=True)
        except:
            pass

    return EvaluationResult(metrics=metrics, artifacts=artifacts)