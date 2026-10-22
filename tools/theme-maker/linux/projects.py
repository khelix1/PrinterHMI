"""Portable editable project archives. No extraction or arbitrary file paths."""
import hashlib
import io
import json
from pathlib import Path
import re
import zipfile
from PIL import Image
from artwork import MAX_SOURCE, cached_source, check_background
from desktop_io import MAX_BYTES, atomic_save_bytes

FORMAT = 'printerhmi.theme-maker.project'
MAX_TEXT = 65536
MAX_ARCHIVE = 6 * 1024 * 1024
EDITOR_KEYS = {'page', 'tab', 'view', 'region', 'grid', 'snap', 'size'}


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('Duplicate project field.')
        result[key] = value
    return result


def invalid_constant(value):
    raise ValueError('Invalid JSON number.')


def document(text):
    if not isinstance(text, str) or not 0 < len(text.encode('utf-8')) <= MAX_TEXT:
        raise ValueError('Project metadata exceeds 64 KiB.')
    p = json.loads(text, object_pairs_hook=unique_object, parse_constant=invalid_constant)
    if not isinstance(p, dict) or set(p) != {'format', 'version', 'theme', 'editor'} or p['format'] != FORMAT or type(p['version']) is not int or p['version'] != 1:
        raise ValueError('Unsupported Theme Maker project.')
    theme = p['theme']; editor = p['editor']
    if not isinstance(theme, dict) or theme.get('schema') != 'printerhmi.theme' or type(theme.get('schema_version')) is not int or theme['schema_version'] not in (1, 2, 3) or len(json.dumps(theme).encode()) > MAX_BYTES:
        raise ValueError('Invalid project theme.')
    if not isinstance(editor, dict) or set(editor) != EDITOR_KEYS:
        raise ValueError('Invalid project workspace.')
    if editor['page'] not in ('dashboard', 'printer', 'files', 'camera', 'tools', 'console', 'drybox', 'settings', 'network', 'telemetry') or editor['tab'] not in ('design', 'layout', 'project') or editor['view'] not in ('device', 'components', 'json') or type(editor['grid']) is not bool or type(editor['snap']) is not bool or type(editor['size']) is not int or editor['size'] not in (4, 8, 16, 32) or not isinstance(editor['region'], str) or len(editor['region']) > 64:
        raise ValueError('Unsupported project workspace settings.')
    if 'background' in theme:
        if theme['schema_version'] != 3:
            raise ValueError('Artwork requires a version 3 theme.')
        check_background(theme['background'])
    elif theme['schema_version'] == 3:
        raise ValueError('Project artwork is missing.')
    return p


def save_request(raw):
    if not isinstance(raw, str) or len(raw.encode('utf-8')) > 2 * MAX_TEXT:
        raise ValueError('Invalid project request size.')
    request = json.loads(raw)
    if not isinstance(request, dict) or request.get('action') != 'save-project' or not re.fullmatch(r'[a-z0-9][a-z0-9-]{1,31}\.phmiproject', str(request.get('filename', ''))):
        raise ValueError('Invalid project save request.')
    document(request.get('text'))
    return request['filename'], request['text']


def pack(text, directory):
    p = document(text)
    source = cached_source(directory, p['theme']['background']['asset']) if 'background' in p['theme'] else None
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr('project.json', text)
        if source is not None:
            archive.writestr('source.png', source)
    return stream.getvalue()


def unpack(data, directory):
    if not 0 < len(data) <= MAX_ARCHIVE:
        raise ValueError('Project exceeds 6 MiB.')
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        infos = archive.infolist(); names = [i.filename for i in infos]
        if len(names) != len(set(names)) or set(names) not in ({'project.json'}, {'project.json', 'source.png'}):
            raise ValueError('Unexpected project archive entries.')
        for info in infos:
            limit = MAX_TEXT if info.filename == 'project.json' else MAX_SOURCE
            if info.flag_bits & 1 or info.file_size > limit or info.is_dir():
                raise ValueError('Project entry exceeds its limit.')
        p = document(archive.read('project.json').decode('utf-8'))
        background = p['theme'].get('background')
        if bool(background) != ('source.png' in names):
            raise ValueError('Project artwork is missing or unexpected.')
        if background:
            source = archive.read('source.png')
            if 'bg-' + hashlib.sha256(source).hexdigest() + '.phmibg' != background['asset']:
                raise ValueError('Project artwork checksum mismatch.')
            with Image.open(io.BytesIO(source)) as image:
                if image.format != 'PNG' or max(image.size) > 1600 or image.width * image.height > 2_560_000 or getattr(image, 'n_frames', 1) != 1:
                    raise ValueError('Invalid project source image.')
                image.verify()
            target = Path(directory); target.mkdir(parents=True, exist_ok=True, mode=0o700)
            atomic_save_bytes(target / (background['asset'] + '.png'), source)
        return p
