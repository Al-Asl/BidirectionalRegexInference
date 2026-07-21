import os
import pathlib

BASE_DIR = pathlib.Path(__file__).parent.resolve()
# only the files that generate stats and use them to create heuristics are sent to the LLM
TARGET_FILES = ["analytic_top_down_samplers.cpp", "stats_extractor.py"]

def format_query_code(dir_path: str, include_all: bool = False) -> str:
    code_dir = pathlib.Path(dir_path)
    res = []
    
    files_to_include = []
    if include_all:
        for ext in ["*.cpp", "*.hpp", "*.h", "*.c", "CMakeLists.txt", "*.py"]:
            for p in code_dir.rglob(ext):
                if 'build' not in p.parts and '.cache' not in p.parts and 'out' not in p.parts:
                    files_to_include.append(p)
    else:
        for ext in TARGET_FILES:
            files_to_include.extend(code_dir.rglob(ext))
        
    for f in files_to_include:
        if not f.is_file(): continue
        rel_path = f.relative_to(code_dir).as_posix()
        content = f.read_text(encoding='utf-8-sig')
        res.append(f"* {rel_path} *:\n@@@\n{content}\n@@@")
        
    return "\n\n".join(res)

def parse_output_code(text: str) -> dict:
    parsed_code = {}
    import re
    pattern = r'\* ([^*:]+) \*:\s*\n@@@\n(.*?)\n@@@'
    for match in re.finditer(pattern, text, re.DOTALL):
        filename = match.group(1).strip()
        content = match.group(2)
        parsed_code[filename] = content
        
    return parsed_code

def save_parsed_output_code(parsed_code: dict, dir_path: str) -> None:
    out_dir = pathlib.Path(dir_path)
    out_dir.mkdir(parents=True, exist_ok=True)
    
    import re
    
    for filename, content in parsed_code.items():
        file_path = out_dir / filename
        file_path.parent.mkdir(parents=True, exist_ok=True)
        
        if "<<<<<<< SEARCH" in content and "=======" in content and ">>>>>>> REPLACE" in content:
            if file_path.exists():
                original_text = file_path.read_text(encoding='utf-8')
                new_text = original_text
                
                blocks = re.findall(r'<<<<<<< SEARCH\n(.*?)\n=======\n(.*?)\n>>>>>>> REPLACE', content, re.DOTALL)
                for search_text, replace_text in blocks:
                    # Handle CRLF vs LF differences
                    search_text_norm = search_text.replace('\r\n', '\n')
                    new_text_norm = new_text.replace('\r\n', '\n')
                    
                    if search_text_norm in new_text_norm:
                        # Need to be careful here: we replace in normalized text, so the result will have LF
                        new_text = new_text_norm.replace(search_text_norm, replace_text.replace('\r\n', '\n'))
                    else:
                        print(f"Warning: Could not find search block in {filename}.")
                
                file_path.write_text(new_text, encoding='utf-8')
            else:
                print(f"Warning: Try to patch non-existent file {filename}")
        else:
            file_path.write_text(content, encoding='utf-8')
