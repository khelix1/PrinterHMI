"""Bounded native export requests and atomic writes, independent of GTK."""
import json
import os
from pathlib import Path
import re
import tempfile

MAX_BYTES = 48 * 1024


def export_request(raw):
    if not isinstance(raw, str) or len(raw.encode('utf-8')) > 2 * MAX_BYTES:
        raise ValueError('Invalid export request size.')
    request = json.loads(raw)
    if not isinstance(request, dict) or request.get('action') != 'export':
        raise ValueError('Unsupported desktop action.')
    name, text = request.get('filename'), request.get('text')
    if not isinstance(name, str) or not re.fullmatch(r'[a-z0-9][a-z0-9-]{1,31}\.phmitheme', name):
        raise ValueError('Invalid theme filename.')
    if not isinstance(text, str) or not 0 < len(text.encode('utf-8')) <= MAX_BYTES:
        raise ValueError('Theme exceeds the 48 KiB firmware limit.')
    document = json.loads(text)
    if not isinstance(document, dict) or document.get('schema') != 'printerhmi.theme':
        raise ValueError('Invalid theme document.')
    return name, text


def atomic_save(filename, text):
    atomic_save_bytes(filename, text.encode('utf-8'))


def atomic_save_bytes(filename, data):
    """Write only to the destination chosen by the native Save dialog."""
    destination = Path(filename)
    fd, temporary = tempfile.mkstemp(prefix='.printerhmi-theme-', dir=destination.parent)
    try:
        with os.fdopen(fd, 'wb') as stream:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, destination)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
