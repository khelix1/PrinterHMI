"""Own one local LVGL child process. Requests never invoke a printer transport."""
import base64
import io
import os
from pathlib import Path
import select
import struct
import subprocess
import time
from PIL import Image

class NativePreview:
    def __init__(self, executable):
        self.process = subprocess.Popen([str(executable)], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.DEVNULL, bufsize=0)

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try: self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill(); self.process.wait()
        self.process.stdin.close(); self.process.stdout.close()

    def _read(self, count, deadline):
        data = bytearray()
        while len(data) < count:
            remaining = deadline-time.monotonic()
            if remaining <= 0 or not select.select([self.process.stdout], [], [], remaining)[0]:
                raise RuntimeError('Native UI did not respond in time.')
            part = os.read(self.process.stdout.fileno(), count-len(data))
            if not part: raise RuntimeError('Native UI process stopped.')
            data.extend(part)
        return bytes(data)

    def hover(self, x, y):
        self.process.stdin.write(('hover %d %d\n' % (x, y)).encode()); self.process.stdin.flush()
        deadline=time.monotonic()+5
        header=bytearray()
        while not header.endswith(b'\n'):
            if len(header)>32: raise RuntimeError('Invalid native hover response.')
            header.extend(self._read(1,deadline))
        cursor=header.decode('ascii').strip().removeprefix('HIT ')
        if cursor not in ('default','pointer','text'): raise RuntimeError('Invalid native cursor.')
        return cursor

    def request(self, line, setup=()):
        if '\n' in line or len(line) > 2000: raise ValueError('Invalid native UI command.')
        self.process.stdin.write((''.join(v+'\n' for v in setup)+line+'\n').encode());self.process.stdin.flush()
        deadline=time.monotonic()+5
        header=bytearray()
        while not header.endswith(b'\n'):
            if len(header)>256: raise RuntimeError('Invalid native frame header.')
            header.extend(self._read(1,deadline))
        parts=header.decode('ascii').split()
        if len(parts)!=15 or parts[0]!='FRAME':raise RuntimeError('Invalid native frame.')
        modal,nozzle,bed,fan,length=map(int,parts[1:6])
        if not 0<=length<=511 or not 0<=modal<=64:raise RuntimeError('Invalid native frame limits.')
        command=self._read(length,deadline).decode('utf-8')
        job_length=int(parts[14])
        if not 0<=job_length<=255:raise RuntimeError('Invalid native job length.')
        job=self._read(job_length,deadline).decode('utf-8')
        raw=self._read(1024*600*4,deadline)
        image=Image.frombytes('RGBA',(1024,600),raw,'raw','BGRA')
        stream=io.BytesIO();image.save(stream,format='PNG')
        return {'image':'data:image/png;base64,'+base64.b64encode(stream.getvalue()).decode(),
                'closed':not modal,'nozzle':nozzle,'bed':bed,'fan':fan,'command':command,'position':list(map(float,parts[6:9])),'zOffset':float(parts[9]),'speed':int(parts[10]),'flow':int(parts[11]),'printState':parts[12],'progress':float(parts[13]),'job':job}
