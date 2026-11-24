# Updated for compact model loading on 2026-11-24; see docs/LOCAL_CHANGES.md.
"""Make a separate compact candidate from an existing verified package. Never publishes."""
from pathlib import Path
import argparse, importlib.util, shutil, subprocess

REPO = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    source, output = args.package.resolve(), args.output.resolve()
    assert output.is_relative_to(REPO/'platforms/out') and not output.exists()
    subprocess.run(['git','-C',str(REPO),'check-ignore','--quiet',str(output)],check=True)
    assert (source/'asset-manifest.json').is_file() and (source/'data/patches.tsv').is_file()
    output.mkdir(parents=True)
    # Explicit package inputs only; never copy user-data, saves, configs or logs.
    for name in ['data', 'licenses']:
        shutil.copytree(source/name, output/name)
    for name in ['NOTICE.txt','README.txt','asset-manifest.json']:
        shutil.copy2(source/name, output/name)
    (output/'bin').mkdir()
    shutil.copy2(REPO/'platforms/out/launcher-build/KotOR-CE.exe',output/'KotOR-CE.exe')
    shutil.copy2(REPO/'platforms/out/k1-private-pc/windows/k1.exe',output/'bin/k1.exe')
    (output/'data/lazy-models-v1').write_text('KLM1\n',encoding='ascii')
    with (output/'README.txt').open('a',encoding='utf-8') as f:
        f.write('\nCOMPACT CANDIDATE: MDL/MDX resources remain compressed. Requested models\n'
                'are validated and reconstructed into delete-on-close temporary streams.\n'
                'Existing v0.2 download/setup size figures above do not apply to this candidate.\n')
    spec=importlib.util.spec_from_file_location('pack',REPO/'platforms/tools/package-portable.py')
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    for file in output.rglob('*'):
        if file.is_file():
            assert not module.private(file.read_bytes()), 'Private content (values suppressed)'
    print('COMPACT_CANDIDATE_PASS')

if __name__ == '__main__':
    main()
