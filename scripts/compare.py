import csv
import sys
def read_csv_to_dict(path):
    """Read CSV into a dict keyed by 'File'."""
    data = {}
    with open(path, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data[row["File"]] = row
    return data
def compare_csvs(original_path, new_path, output_path):
    original = read_csv_to_dict(original_path)
    new = read_csv_to_dict(new_path)
    fieldnames = [
        "File",
        "RE_original", "Cost_original", "REs_original", "Time_original",
        "RE_new", "Cost_new", "REs_new", "Time_new",
        "Cost_ratio", "REs_ratio", "Time_ratio"
    ]
    with open(output_path, "w", newline='', encoding='utf-8') as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        for file_name in original:
            if file_name not in new:
                continue  # skip unmatched entries
            o = original[file_name]
            n = new[file_name]
            
            # Check if either RE contains "not_found"
            has_not_found = "not_found" in o["RE"].lower() or "not_found" in n["RE"].lower()
            
            cost_original = float(o["Cost"])
            cost_new = float(n["Cost"])
            res_original = float(o["REs"])
            res_new = float(n["REs"])
            time_original = float(o["Time (s)"])
            time_new = float(n["Time (s)"])
            
            # Calculate ratios only if "not_found" is not present
            cost_ratio = cost_new / cost_original if not has_not_found else ""
            res_ratio = res_new / res_original if not has_not_found else ""
            time_ratio = time_new / time_original if not has_not_found else ""
            
            writer.writerow({
                "File": file_name,
                "RE_original": o["RE"],
                "Cost_original": cost_original,
                "REs_original": res_original,
                "Time_original": time_original,
                "RE_new": n["RE"],
                "Cost_new": cost_new,
                "REs_new": res_new,
                "Time_new": time_new,
                "Cost_ratio": cost_ratio,
                "REs_ratio": res_ratio,
                "Time_ratio": time_ratio,
            })
def main():
    if len(sys.argv) != 4:
        print("Usage: python compare.py <original.csv> <new.csv> <output.csv>")
        sys.exit(1)
    original_path = sys.argv[1]
    new_path = sys.argv[2]
    output_path = sys.argv[3]
    compare_csvs(original_path, new_path, output_path)
    print(f"Comparison written to {output_path}")
if __name__ == "__main__":
    main()