"""tests/gen_variants_test.py <tools dir>: gen.py's code-cheat variants (patches/0010, 0012, 0016) with the real lifter
on a stand-in module (nops): a variant branching somewhere new makes its target a function start (else the game
library linked to a function that doesn't exist and wouldn't load), a cave extends the module's code and starts a
function, a malformed file leaves everything as it was. Run with the AppImage's Python (it has gen.py's modules):
  work/AppDir/python/bin/python3 -I tests/gen_variants_test.py work/src/tools
"""
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.abspath(sys.argv[1]))
import gen  # noqa: E402
from lifter import Lifter  # noqa: E402

BASE = 0x7000000000
NOP = 0xD503201F
results = []


def check(cond, name):
    results.append(bool(cond))
    print(('ok   ' if cond else 'FAIL ') + name)


class Module:  # (what load_code_variants uses of gen.Module)
    def __init__(self, n_words=64):
        self.name, self.base = 'main', BASE
        self.lo, self.hi = BASE, BASE + 4 * n_words
        self.words = tuple([NOP] * n_words)
        self.lifter = Lifter(BASE, 'c', False)


def variants_file(d):
    f = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
    json.dump(d, f)
    f.close()
    return f.name


def load(m, d):
    gen.RANDO_CODE.clear()
    return gen.load_code_variants(m, variants_file(d))


# a bl to 0x50 and a b to 0x80 compiled in at 0x10 / 0x20: both targets become function starts
m = Module()
kept = load(m, {'code': {'0x10': ['0x94000010'], '0x20': ['0x14000018']}})
check(len(kept) == 2 and gen.RANDO_CODE.get(0x10) == [0x94000010], 'variants kept and in RANDO_CODE')
check(BASE + 0x50 in getattr(m, 'variant_starts', set()), 'a variant bl\'s target is a function start')
check(BASE + 0x80 in getattr(m, 'variant_starts', set()), 'a variant b\'s target is a function start')

# a branch out of the module's code (no function of ours there): not a start - jump() dispatches it at run time
m = Module()
load(m, {'code': {'0x10': ['0x94100000']}})  # (bl +0x400000: past the module)
check(not any(t >= m.hi for t in getattr(m, 'variant_starts', set())), 'a target outside the code is not made a start')

# a cave right after the code: the code grows, the cave's first word starts a function, its bl from the code is kept
m = Module()
kept = load(m, {'code': {'0x10': ['0x9400003C']}, 'cave': {'0x100': '0x52800020', '0x104': '0xd65f03c0'}})
check(m.hi == BASE + 0x108 and len(m.words) == 0x108 // 4, 'the cave extends the module\'s code')
check(m.words[0x100 // 4] == 0x52800020 and m.words[0x104 // 4] == 0xD65F03C0, 'the cave\'s words are in place')
check(getattr(m, 'cave_starts', []) == [BASE + 0x100], 'the cave starts a function')
check((0x100, 0x52800020) in kept and (0x104, 0xD65F03C0) in kept, 'the cave\'s words are in the compiled-in list')

# a malformed file: nothing compiled in, the module unchanged
m = Module()
p = tempfile.NamedTemporaryFile('w', suffix='.json', delete=False)
p.write('{not json')
p.close()
gen.RANDO_CODE.clear()
kept = gen.load_code_variants(m, p.name)
check(kept == [] and not gen.RANDO_CODE and m.hi == BASE + 0x100, 'a malformed file changes nothing')

print(f'gen variants: {sum(results)}/{len(results)} passed')
sys.exit(0 if all(results) else 1)
