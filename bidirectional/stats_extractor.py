import sys
import os
import pathlib
import json
import math

from language_system import LanguageSystem
from benchmark_data import BenchmarkResults, parse_benchmark_file, compute_mean_stddev

def generate_stats():
    if len(sys.argv) < 2:
        print("Usage: python stats_extractor.py <path_to_benchmark_results.json>")
        return

    json_path = sys.argv[1]
    
    if not os.path.exists(json_path):
        print(f"Warning: {json_path} not found")
        # Write an empty header so compilation doesn't fail
        with open("include/generated_stats.hpp", "w") as f:
            f.write("#pragma once\n// No stats generated\n")
        return

    # Load results using the new class
    benchmark_data = BenchmarkResults(json_path)
    results = benchmark_data.get_results()

    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    solution_extra_words_pct_list = []
    solution_word_length_pct_list = []
    union_left_words_pct_list = []
    union_left_word_length_pct_list = []
    concat_left_word_splits_pct_list = []

    # Compute stats using language_system if needed
    for bench_file, entry in results.items():
        if not entry.success:
            continue
            
        full_bench_path = os.path.join(base_dir, bench_file)
        if not os.path.exists(full_bench_path):
            continue
            
        # Parse benchmark file
        pos, neg = parse_benchmark_file(full_bench_path)
        # Build language system
        ls = LanguageSystem(pos, neg)
        
        ic = ls.get_ic()
        ic_size = len(ic)
        if ic_size == 0:
            continue
            
        max_word_length = max((len(w) for w in ic), default=1)
        if max_word_length == 0: max_word_length = 1
        
        pos_set = set(pos)
        pos_indices = {i for i, w in enumerate(ic) if w in pos_set}
        
        guide_table = ls.get_guide_table()
        max_word_splits = max((len(splits) for splits in guide_table), default=1)
        
        for solution in entry.solutions:
            node_depths = solution.get_node_depths()
            root_decomp = solution.get_root_decomp()
            
            if root_decomp:
                root_bits = root_decomp.get_root_bits()
                extra_bits = [b for b in root_bits if b not in pos_indices]
                
                solution_extra_words_pct_list.append(len(extra_bits) / ic_size)
                
                for b in extra_bits:
                    solution_word_length_pct_list.append(len(ic[b]) / max_word_length)
                    
            for d in solution.decomposition:
                depth = node_depths.get(d.index, 999)
                if depth > 3:
                    continue
                    
                if d.op == "O":
                    left_bits = d.get_left_bits()
                    union_left_words_pct_list.append(len(left_bits) / ic_size)
                    
                    for b in left_bits:
                        union_left_word_length_pct_list.append(len(ic[b]) / max_word_length)
                
                elif d.op == "C":
                    root_bits = d.get_root_bits()
                    left_bits_set = set(d.get_left_bits())
                    right_bits_set = set(d.get_right_bits())
                    
                    for b in root_bits:
                        if b >= len(guide_table): continue
                        splits = guide_table[b]
                        
                        for l_idx, r_idx in splits:
                            if l_idx in left_bits_set and r_idx in right_bits_set:
                                left_splits_count = len(guide_table[l_idx])
                                concat_left_word_splits_pct_list.append(left_splits_count / max_word_splits)
    
    # Calculate means and stddevs
    s_extra_mean, s_extra_std = compute_mean_stddev(solution_extra_words_pct_list)
    s_len_mean, s_len_std = compute_mean_stddev(solution_word_length_pct_list)
    u_words_mean, u_words_std = compute_mean_stddev(union_left_words_pct_list)
    u_len_mean, u_len_std = compute_mean_stddev(union_left_word_length_pct_list)
    c_splits_mean, c_splits_std = compute_mean_stddev(concat_left_word_splits_pct_list)

    # Generate the header file with statistics
    with open("include/generated_stats.hpp", "w") as f:
        f.write("#pragma once\n\n")
        
        f.write("// precentage of the IC size\n")
        f.write(f"constexpr double SOLUTION_SET_MEAN_EXTRA_WORDS_PCT = {s_extra_mean};\n")
        f.write(f"constexpr double SOLUTION_SET_STDDEV_EXTRA_WORDS_PCT = {s_extra_std};\n\n")
        
        f.write("// precentage of the largest word in length\n")
        f.write(f"constexpr double SOLUTION_SET_MEAN_WORD_LENGTH_PCT = {s_len_mean};\n")
        f.write(f"constexpr double SOLUTION_SET_STDDEV_WORD_LENGTH_PCT = {s_len_std};\n\n")
        
        f.write("// precentage of the IC size\n")
        f.write(f"constexpr double UNION_LEFT_MEAN_WORDS_PCT = {u_words_mean};\n")
        f.write(f"constexpr double UNION_LEFT_STDDEV_WORDS_PCT = {u_words_std};\n\n")
        
        f.write("// precentage of the largest word in length\n")
        f.write(f"constexpr double UNION_LEFT_MEAN_WORD_LENGTH_PCT = {u_len_mean};\n")
        f.write(f"constexpr double UNION_LEFT_STDDEV_WORD_LENGTH_PCT = {u_len_std};\n\n")
        
        f.write("// precentage of the highest word splits in the language\n")
        f.write(f"constexpr double CONCAT_LEFT_MEAN_WORD_SPLITS_PCT = {c_splits_mean};\n")
        f.write(f"constexpr double CONCAT_LEFT_STDDEV_WORD_SPLITS_PCT = {c_splits_std};\n")

if __name__ == "__main__":
    generate_stats()

