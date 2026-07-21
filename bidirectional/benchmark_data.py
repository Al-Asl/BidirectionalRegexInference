import json
import math
from dataclasses import dataclass
from typing import List, Dict, Optional, Any, Tuple

def parse_benchmark_file(filepath: str) -> Tuple[List[str], List[str]]:
    pos = []
    neg = []
    with open(filepath, 'r') as f:
        lines = [l.strip() for l in f if l.strip()]
    
    current_list = None
    for line in lines:
        if line == '++':
            current_list = pos
        elif line == '--':
            current_list = neg
        elif line.startswith('"') and line.endswith('"'):
            if current_list is not None:
                current_list.append(line[1:-1])
    return pos, neg

def get_set_bits(blocks: Optional[List[int]]) -> List[int]:
    bits = []
    if not blocks:
        return bits
    for i, block in enumerate(blocks):
        for j in range(64):
            if (block & (1 << j)) != 0:
                bits.append(i * 64 + j)
    return bits

def compute_mean_stddev(data: List[float], default_mean: float = 0.5, default_stddev: float = 0.15) -> Tuple[float, float]:
    if not data:
        return default_mean, default_stddev
    mean = sum(data) / len(data)
    variance = sum((x - mean) ** 2 for x in data) / len(data)
    stddev = math.sqrt(variance)
    return mean, stddev

@dataclass
class Decomposition:
    index: int
    op: str
    root: List[int]
    left: Optional[List[int]] = None
    right: Optional[List[int]] = None

    def get_root_bits(self) -> List[int]:
        return get_set_bits(self.root)
        
    def get_left_bits(self) -> List[int]:
        return get_set_bits(self.left)
        
    def get_right_bits(self) -> List[int]:
        return get_set_bits(self.right)

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Decomposition':
        return cls(
            index=data.get("index"),
            op=data.get("op"),
            root=data.get("root", []),
            left=data.get("left"),
            right=data.get("right")
        )

@dataclass
class Solution:
    all_res: int
    unique_res: int
    running_time_s: float
    cost: int
    re: str
    decomposition: List[Decomposition]

    def get_root_decomp(self) -> Optional[Decomposition]:
        for d in self.decomposition:
            if d.index == 1:
                return d
        return None

    def get_node_depths(self) -> Dict[int, int]:
        node_depths = {}
        root_decomp = self.get_root_decomp()
        if not root_decomp:
            return node_depths
            
        queue = [(root_decomp, 1)]
        node_depths[root_decomp.index] = 1
        
        while queue:
            curr, depth = queue.pop(0)
            left_key = tuple(curr.left) if curr.left else None
            right_key = tuple(curr.right) if curr.right else None
            
            for key in (left_key, right_key):
                if key:
                    children = [d for d in self.decomposition if tuple(d.root) == key]
                    for child in children:
                        if child.index not in node_depths:
                            node_depths[child.index] = depth + 1
                            queue.append((child, depth + 1))
        return node_depths

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Solution':
        return cls(
            all_res=data.get("all_res"),
            unique_res=data.get("unique_res"),
            running_time_s=data.get("running_time_s"),
            cost=data.get("cost"),
            re=data.get("re"),
            decomposition=[Decomposition.from_dict(d) for d in data.get("decomposition", [])]
        )

@dataclass
class BenchmarkEntry:
    benchmark_file: str
    success: bool
    solutions: List[Solution]

    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'BenchmarkEntry':
        return cls(
            benchmark_file=data.get("benchmark_file"),
            success=data.get("success"),
            solutions=[Solution.from_dict(s) for s in data.get("solutions", [])]
        )

class BenchmarkResults:
    def __init__(self, filepath: str):
        with open(filepath, 'r') as f:
            raw_data = json.load(f)
        self.results: Dict[str, BenchmarkEntry] = {
            k: BenchmarkEntry.from_dict(v) for k, v in raw_data.items()
        }

    def get_results(self) -> Dict[str, BenchmarkEntry]:
        return self.results

