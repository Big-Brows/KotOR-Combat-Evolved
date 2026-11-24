"""Classify Red chains using the preserved JKA SP spinning-animation list.

Generate selectors only; no authored animation assets or game execution.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess

repo=Path(__file__).resolve().parents[2]
out=repo/'platforms/out/red-spin-fix'
source=repo/'platforms/out/jka-animation-import/research/openjk/code/game/bg_pmove.cpp'
text=source.read_text()
signature='qboolean PM_SpinningSaberAnim( int anim )\n{'
assert text.count(signature)==1
block=text.split(signature,1)[1].split('//level 4',1)[0]
spins=set(re.findall(r'case (BOTH_T3_[A-Z_]+):',block))
assert len(spins)==6
contract=(repo/'src/game/clientcore/include/k1redchain.inc').read_text()
transitions={(int(a),int(b)):clip for a,b,clip in re.findall(r'\{"s_r(\d)(\d)","y_r\d\d","([^"]+)"',contract)}
assert len(transitions)==49
identity=json.loads((repo/'platforms/out/red-stance/generation-report.json').read_text())
lines=['// JKA SP PM_SpinningSaberAnim: strong links requiring full-body playback.',
       'constexpr bool redSpinChains[7][7]={']
pairs=[]
for outgoing in range(7):
    row=[]
    for incoming in range(7):
        name=f's_r{outgoing}{incoming}'
        transition=transitions[outgoing,incoming]
        if transition!='direct':assert identity['clips'][name]['source'][0]==transition
        spinning=transition in spins
        row.append('true' if spinning else 'false')
        if spinning:pairs.append(name)
    lines.append('    {'+', '.join(row)+'},')
lines.append('};')
(repo/'src/game/clientcore/include/k1redspins.inc').write_text('\n'.join(lines)+'\n')
subprocess.run(['git','-C',str(repo),'check-ignore','--quiet',str(out/'spin-contract.json')],check=True)
out.mkdir(exist_ok=True)
(out/'spin-contract.json').write_text(json.dumps({'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
    'red_contract_sha256':hashlib.sha256(contract.encode()).hexdigest(),
    'spinning_source_clips':sorted(spins),'full_body_chain_pairs':pairs},indent=2))
print('RED_SPIN_CONTRACT_PASS',len(spins),'source spin clips;',len(pairs),'full-body pairs',flush=True)
