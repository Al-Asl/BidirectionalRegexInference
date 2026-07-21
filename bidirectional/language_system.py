class LanguageSystem:
    def __init__(self, pos: list[str], neg: list[str]):
        # Generating infix-closure (ic) of the input strings
        ic_set = set()
        for word in pos + neg:
            length = len(word)
            for l in range(length + 1):
                for index in range(length - l + 1):
                    ic_set.add(word[index:index+l])
        
        # Sort by length first, then lexicographically (Shortlex ordering)
        self.ic = sorted(list(ic_set), key=lambda x: (len(x), x))
        
        self.alphabet_size = -1
        for word in self.ic:
            if len(word) > 1:
                break
            self.alphabet_size += 1
            
        index_map = {w: i for i, w in enumerate(self.ic)}
        
        self.guide_table = [[] for _ in range(len(self.ic))]
        
        for i, word in enumerate(self.ic):
            length = len(word)
            for j in range(1, length):
                left_idx = index_map[word[:j]]
                right_idx = index_map[word[j:]]
                self.guide_table[i].append((left_idx, right_idx))
            
            if i != 0:
                self.guide_table[i].append((0, i))
                self.guide_table[i].append((i, 0))
                
        # The first element is epsilon. 
        # Add epsilon * epsilon = epsilon to the guide table
        self.guide_table[0].append((0, 0))
        
        self.suffixes = [[] for _ in range(len(self.ic))]
        self.prefixes = [[] for _ in range(len(self.ic))]
        
        for i, gt_row in enumerate(self.guide_table):
            for left_idx, right_idx in gt_row:
                self.suffixes[left_idx].append((right_idx, i))
                self.prefixes[right_idx].append((left_idx, i))

    def get_guide_table(self):
        return self.guide_table

    def get_suffixes(self):
        return self.suffixes

    def get_prefixes(self):
        return self.prefixes

    def get_ic(self):
        return self.ic

    def get_alphabet_size(self):
        return self.alphabet_size
