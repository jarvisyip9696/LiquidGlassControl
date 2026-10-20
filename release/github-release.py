"""Publish explicitly allowlisted release assets using the existing Git credential."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
VERSION = re.search(r'^#define LG_VERSION "([^"]+)"', (ROOT / 'src/app_version.h').read_text(), re.M)[1]
TAG = 'v' + VERSION
REPO = '/repos/jarvisyip9696/LiquidGlassControl'
env = dict(os.environ, GIT_TERMINAL_PROMPT='0', GCM_INTERACTIVE='Never')
credential = subprocess.run(['git', '-c', 'credential.interactive=false', 'credential', 'fill'],
    input='protocol=https\nhost=github.com\n\n', text=True, capture_output=True, env=env, check=True)
values = dict(line.split('=', 1) for line in credential.stdout.splitlines() if '=' in line)
if not values.get('password'):
    raise SystemExit('No existing GitHub credential.')

def api(path, method='GET', data=None, raw=None):
    url = path if path.startswith('https://') else 'https://api.github.com' + path
    if not url.startswith(('https://api.github.com/', 'https://uploads.github.com/')):
        raise ValueError('Unexpected API host')
    headers = {'Authorization': 'Bearer ' + values['password'], 'Accept': 'application/vnd.github+json',
        'User-Agent': 'LiquidGlass-release', 'X-GitHub-Api-Version': '2022-11-28'}
    if data is not None:
        raw = json.dumps(data).encode()
        headers['Content-Type'] = 'application/json'
    elif raw is not None:
        headers['Content-Type'] = 'application/octet-stream'
    with urllib.request.urlopen(urllib.request.Request(url, data=raw, headers=headers, method=method), timeout=60) as response:
        return json.load(response)

def release():
    return next((item for item in api(REPO + '/releases?per_page=100') if item['tag_name'] == TAG), None)

def assets():
    folder = ROOT / 'dist' / VERSION
    names = [f'LiquidGlassControl-{VERSION}-win11-x64.zip', f'LiquidGlassControl-{VERSION}-win11-x64.exe',
        f'LiquidGlassControl-{VERSION}-source.zip', 'LGEngine-2.7.14.0-source.zip', 'SHA256SUMS.txt']
    paths = [folder / name for name in names]
    if not all(path.is_file() for path in paths):
        raise SystemExit('Missing release asset')
    return paths

mode = sys.argv[1]
if mode == 'draft':
    item = release()
    if item is None:
        sha = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
        item = api(REPO + '/releases', 'POST', {'tag_name': TAG, 'target_commitish': sha,
            'name': 'Liquid Glass Control ' + VERSION, 'draft': True, 'prerelease': True,
            'body': (ROOT / 'docs' / ('RELEASE-' + VERSION + '.md')).read_text(encoding='utf-8')})
    if not item['draft']:
        raise SystemExit('Refusing to modify downloads on an already published release')
    existing = {asset['name'] for asset in item['assets']}
    for path in assets():
        if path.name not in existing:
            from urllib.parse import quote
            uploaded = api(item['upload_url'].split('{')[0] + '?name=' + quote(path.name), 'POST', raw=path.read_bytes())
            print(json.dumps({'uploaded': uploaded['name'], 'bytes': uploaded['size']}), flush=True)
elif mode in ('verify', 'publish'):
    item = release()
    if item is None:
        raise SystemExit('Release missing')
    remote = {asset['name']: asset for asset in item['assets']}
    for path in assets():
        asset = remote[path.name]
        expected = 'sha256:' + hashlib.sha256(path.read_bytes()).hexdigest()
        if asset['size'] != path.stat().st_size or asset.get('digest') != expected:
            raise SystemExit('Release digest mismatch: ' + path.name)
    if mode == 'publish':
        item = api(REPO + '/releases/' + str(item['id']), 'PATCH', {'draft': False, 'prerelease': True,
            'name': 'Liquid Glass Control ' + VERSION,
            'body': (ROOT / 'docs' / ('RELEASE-' + VERSION + '.md')).read_text(encoding='utf-8')})
    print(json.dumps({'url': item['html_url'], 'draft': item['draft'], 'prerelease': item['prerelease'],
        'verified_assets': list(remote)}))
else:
    raise SystemExit('Expected draft, verify or publish')
