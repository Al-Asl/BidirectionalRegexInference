import os
import re
import subprocess
import csv
import sys

# Regex patterns
RE_PATTERN = re.compile(r'RE:\s*"(.*)"')
COST_PATTERN = re.compile(r'Cost:\s*(\d+)')
RES_PATTERN = re.compile(r'^REs:\s*([0-9]+)\s*$', re.MULTILINE)
TIME_PATTERN = re.compile(r'Running Time:\s*([\d.]+)\s*s')

def natural_sort_key(s):
    return [
        int(text) if text.isdigit() else text.lower()
        for text in re.split(r'(\d+)', s)
    ]

def run_bdres_on_directory(directory, csv_writer, csv_file):
    results = []

    # get, sort, and enumerate all .txt files
    txt_files = sorted(
        (f for f in os.listdir(directory) if f.endswith(".txt")),
        key=natural_sort_key
    )


    total = len(txt_files)

    for idx, filename in enumerate(txt_files, start=1):
        filepath = os.path.join(directory, filename)
        base_name = os.path.splitext(filename)[0]

        cmd = [
            "../BidirectionalRegexInference/build/Release/RegexInference.exe",
            filepath,
            "1", "1", "1", "1", "1", "500"
        ]

        try:
            print(f"[{idx}/{total}] running {filename}")

            completed = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                check=True
            )
            output = completed.stdout

            re_match = RE_PATTERN.search(output)
            cost_match = COST_PATTERN.search(output)
            res_match = RES_PATTERN.search(output)
            time_match = TIME_PATTERN.search(output)

            if not all([re_match, cost_match, res_match, time_match]):
                print(f"Warning: Could not fully parse output for {filename}")
                continue

            row = [
                base_name,
                re_match.group(1),
                int(cost_match.group(1)),
                int(res_match.group(1)),
                float(time_match.group(1))
            ]

            # write row immediately
            csv_writer.writerow(row)
            csv_file.flush()
            os.fsync(csv_file.fileno())

            results.append(row)
            print(row)

        except subprocess.CalledProcessError as e:
            print(f"Error running RegexInference on {filename}: {e}")

    return results


def print_table(results):
    headers = ["File", "RE", "Cost", "REs", "Time (s)"]
    col_widths = [
        max(len(str(row[i])) for row in ([headers] + results))
        for i in range(len(headers))
    ]

    def print_row(row):
        print(" | ".join(str(row[i]).ljust(col_widths[i]) for i in range(len(row))))

    print_row(headers)
    print("-+-".join("-" * w for w in col_widths))
    for row in results:
        print_row(row)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python run_bdres.py <directory>")
        sys.exit(1)

    directory = sys.argv[1]

    # CSV name = directory name only
    dir_name = os.path.basename(os.path.normpath(directory))
    csv_name = f"{dir_name}.csv"

    # create CSV immediately
    with open(csv_name, "w", newline="", buffering=1) as csv_file:
        writer = csv.writer(csv_file)
        headers = ["File", "RE", "Cost", "REs", "Time (s)"]
        writer.writerow(headers)
        csv_file.flush()
        os.fsync(csv_file.fileno())

        results = run_bdres_on_directory(directory, writer, csv_file)

    print_table(results)
    print(f"\nSaved results to {csv_name}")
