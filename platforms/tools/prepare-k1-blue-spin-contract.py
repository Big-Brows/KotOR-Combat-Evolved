"""Classify blue chains from the preserved JKA SP spinning-animation list.

Generates selectors only, no animation/asset changes or game execution.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess

repo=Path(__file__).resolve().parents[2]
root=repo/'platforms/out/blue-stance'
source=repo/'platforms/out/jka-animation-import/research/openjk/code/game/bg_pmove.cpp'
text=source.read_text()
signature='qboolean PM_SpinningSaberAnim( int anim )\n{'
assert text.count(signature)==1
block=text.split(signature,1)[1].split('//level 2',1)[0]
spins=set(re.findall(r'case (BOTH_T1_[A-Z_]+):',block))
assert len(spins)==12
report=json.loads((root/'generation-report.json').read_text())
lines=['// JKA SP PM_SpinningSaberAnim: blue links requiring full-body playback.',
       'constexpr bool blueSpinChains[7][7]={']
pairs=[]
for outgoing in range(7):
    row=[]
    for incoming in range(7):
        name=f's_b{outgoing}{incoming}'
        clip=report['clips'][name]
        spinning=clip['source'][0] in spins
        row.append('true' if spinning else 'false')
        if spinning:pairs.append(name)
    lines.append('    {'+', '.join(row)+'},')
lines.append('};')
dest=repo/'src/game/clientcore/include/k1bluespins.inc'
dest.write_text('\n'.join(lines)+'\n')
out=root/'spin-investigation'
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'spin-contract.json')],check=True)
out.mkdir(exist_ok=True)
(out/'spin-contract.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'spinning_source_clips':sorted(spins),'full_body_chain_pairs':pairs},indent=2))
print('BLUE_SPIN_CONTRACT_PASS',len(spins),'source spin clips;',len(pairs),'full-body pairs',flush=True)
