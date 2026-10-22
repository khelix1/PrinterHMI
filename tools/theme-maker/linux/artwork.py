"""Bounded artwork decoding, screen composition and portable theme bundles."""
import array
import base64
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import sys
import warnings
import zipfile
import zlib
from PIL import Image, ImageOps
from desktop_io import MAX_BYTES

WIDTH, HEIGHT = 1024, 600
PIXEL_BYTES = WIDTH * HEIGHT * 2
MAX_SOURCE = 4 * 1024 * 1024
MAX_INPUT = 12 * 1024 * 1024
ASSET = re.compile(r'bg-[a-f0-9]{64}\.phmibg')
HEADER = struct.Struct('<8sHHII')


def check_background(background):
    if not isinstance(background, dict) or not ASSET.fullmatch(str(background.get('asset', ''))):
        raise ValueError('Invalid artwork reference.')
    if background.get('fit') not in ('cover', 'contain', 'stretch'):
        raise ValueError('Choose Cover, Contain or Stretch.')
    for key in ('position_x', 'position_y', 'overlay_opacity'):
        if type(background.get(key)) is not int or not 0 <= background[key] <= 100:
            raise ValueError('Artwork position and overlay must be 0–100.')
    if not re.fullmatch(r'#[a-fA-F0-9]{6}', str(background.get('overlay_color', ''))):
        raise ValueError('Invalid overlay color.')


def source_image(data):
    if not 0 < len(data) <= MAX_INPUT:
        raise ValueError('Choose an image no larger than 12 MiB.')
    try:
        with warnings.catch_warnings():
            warnings.simplefilter('error', Image.DecompressionBombWarning)
            with Image.open(io.BytesIO(data)) as original:
                if original.format not in ('PNG', 'JPEG', 'WEBP') or original.width * original.height > 16_000_000:
                    raise ValueError('Choose PNG, JPEG or WebP, no larger than 16 megapixels.')
                if getattr(original, 'n_frames', 1) != 1:
                    raise ValueError('Animated images are not supported.')
                image = ImageOps.exif_transpose(original).convert('RGBA')
    except (OSError, Image.DecompressionBombError, Image.DecompressionBombWarning) as error:
        raise ValueError('The image could not be opened safely. Choose a valid PNG, JPEG or WebP up to 16 megapixels.') from error
    image.thumbnail((1600, 1600), Image.Resampling.LANCZOS)
    stream = io.BytesIO(); image.save(stream, 'PNG')
    data = stream.getvalue()
    if len(data) > MAX_SOURCE:
        raise ValueError('Image is too complex after normalization; choose a smaller image.')
    return data, image.size


def store_source(data, directory):
    png, size = source_image(data)
    name = 'bg-' + hashlib.sha256(png).hexdigest() + '.phmibg'
    directory = Path(directory); directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    target = directory / (name + '.png')
    if not target.is_file() or target.stat().st_size != len(png) or target.read_bytes() != png:
        from desktop_io import atomic_save_bytes
        atomic_save_bytes(target, png)
    return name, size


def cached_source(directory, name):
    if not isinstance(name, str) or not ASSET.fullmatch(name):
        raise ValueError('Invalid artwork reference.')
    path = Path(directory) / (name + '.png')
    if not path.is_file() or path.stat().st_size > MAX_SOURCE:
        raise ValueError('Artwork source is missing. Import its theme bundle or choose the image again.')
    data = path.read_bytes()
    if 'bg-' + hashlib.sha256(data).hexdigest() + '.phmibg' != name:
        raise ValueError('Cached artwork is damaged. Choose the image again.')
    return data


def compose(png, background, color):
    check_background(background)
    if not re.fullmatch(r'#[a-fA-F0-9]{6}', str(color)):
        raise ValueError('Invalid canvas color.')
    with Image.open(io.BytesIO(png)) as opened:
        if opened.width * opened.height > 2_560_000:
            raise ValueError('Artwork source is too large.')
        source = opened.convert('RGBA')
    base = Image.new('RGBA', (WIDTH, HEIGHT), color)
    if background['fit'] == 'stretch':
        size = (WIDTH, HEIGHT)
    else:
        ratios = (WIDTH / source.width, HEIGHT / source.height)
        scale = max(ratios) if background['fit'] == 'cover' else min(ratios)
        size = (max(1, round(source.width * scale)), max(1, round(source.height * scale)))
    # Bound pathological panoramas before allocating a huge intermediate.
    if size[0] * size[1] > 8_000_000 or max(size) > 16384:
        raise ValueError('Image is too panoramic for Cover. Choose Contain or a different image.')
    source = source.resize(size, Image.Resampling.LANCZOS)
    x = round((WIDTH - size[0]) * background['position_x'] / 100)
    y = round((HEIGHT - size[1]) * background['position_y'] / 100)
    base.paste(source, (x, y), source)
    overlay = Image.new('RGBA', base.size, background['overlay_color'])
    base = Image.blend(base, overlay, background['overlay_opacity'] / 100).convert('RGB')
    return base


def encode_asset(image):
    raw = image.tobytes()
    pixels = array.array('H', (((raw[i] >> 3) << 11) | ((raw[i+1] >> 2) << 5) | (raw[i+2] >> 3) for i in range(0, len(raw), 3)))
    if sys.byteorder != 'little': pixels.byteswap()
    payload = pixels.tobytes()
    return HEADER.pack(b'PHMIBG01', WIDTH, HEIGHT, len(payload), zlib.crc32(payload)) + payload


def validate_asset(data):
    if len(data) != HEADER.size + PIXEL_BYTES:
        raise ValueError('Invalid panel artwork size.')
    magic, width, height, count, crc = HEADER.unpack_from(data)
    if (magic, width, height, count) != (b'PHMIBG01', WIDTH, HEIGHT, PIXEL_BYTES) or zlib.crc32(data[HEADER.size:]) != crc:
        raise ValueError('Panel artwork is damaged.')


def preview(project, directory):
    background = project['background']
    image = compose(cached_source(directory, background['asset']), background, project['palette']['background'])
    stream = io.BytesIO(); image.save(stream, 'PNG')
    return 'data:image/png;base64,' + base64.b64encode(stream.getvalue()).decode('ascii')


def bundle(text, directory):
    if len(text.encode('utf-8')) > MAX_BYTES:
        raise ValueError('Theme metadata exceeds 48 KiB.')
    project = json.loads(text); background = project['background']; check_background(background)
    if project.get('schema') != 'printerhmi.theme' or project.get('schema_version') != 3 or not re.fullmatch(r'[a-z0-9][a-z0-9-]{1,31}', str(project.get('id', ''))):
        raise ValueError('Invalid artwork theme.')
    png = cached_source(directory, background['asset'])
    asset = encode_asset(compose(png, background, project['palette']['background']))
    background['asset'] = 'bg-' + hashlib.sha256(asset).hexdigest() + '.phmibg'
    folder = 'PrinterHMI/themes/'
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
        archive.writestr(folder + project['id'] + '.phmitheme', json.dumps(project, ensure_ascii=False, indent=2) + '\n')
        archive.writestr(folder + background['asset'], asset)
        archive.writestr('ThemeMaker/source.png', png)
        archive.writestr('INSTALL.txt', 'Extract this ZIP to the root of the panel SD card.\nRequires artwork-capable firmware.\nImport this ZIP into Theme Maker to keep editing the source artwork.\n')
    return stream.getvalue()


def import_bundle(data, directory):
    if not 0 < len(data) <= 8 * 1024 * 1024:
        raise ValueError('Theme bundle must be no larger than 8 MiB.')
    with zipfile.ZipFile(io.BytesIO(data)) as archive:
        infos = archive.infolist(); names = [i.filename for i in infos]
        if len(names) != len(set(names)) or len(names) not in (3, 4):
            raise ValueError('Unexpected theme bundle entries.')
        for info in infos:
            if info.flag_bits & 1 or info.file_size > MAX_SOURCE or info.is_dir():
                raise ValueError('Invalid theme bundle entry.')
        themes = [n for n in names if re.fullmatch(r'PrinterHMI/themes/[a-z0-9][a-z0-9-]{1,31}\.phmitheme', n)]
        if len(themes) != 1 or archive.getinfo(themes[0]).file_size > MAX_BYTES:
            raise ValueError('Bundle must contain one bounded theme document.')
        text = archive.read(themes[0]).decode('utf-8'); project = json.loads(text)
        if project.get('schema') != 'printerhmi.theme' or project.get('schema_version') != 3:
            raise ValueError('Unsupported artwork theme.')
        check_background(project.get('background')); asset_name = project['background']['asset']
        allowed = {themes[0], 'PrinterHMI/themes/' + asset_name, 'ThemeMaker/source.png', 'INSTALL.txt'}
        if set(names) - allowed or not allowed - {'INSTALL.txt'} <= set(names):
            raise ValueError('Unexpected artwork paths; bundle rejected.')
        asset = archive.read('PrinterHMI/themes/' + asset_name); validate_asset(asset)
        if 'bg-' + hashlib.sha256(asset).hexdigest() + '.phmibg' != asset_name:
            raise ValueError('Artwork checksum does not match its filename.')
        source, _ = store_source(archive.read('ThemeMaker/source.png'), directory)
        project['background']['asset'] = source
        return project
