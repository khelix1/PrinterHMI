#!/usr/bin/env python3
"""Compile the real WebSocket/HTTP command adapters with typed transport fixtures."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
def function(source,name):
    # Firmware functions close at column zero; nested blocks are indented.
    pos=source.index(name+'(')
    start=source.rfind('\n',0,pos)+1
    # Function return type/signature starts on the same line in these adapters.
    end=source.index('\n}\n',pos)+3
    return source[start:end]
ws=(root/'main/moonraker_live_websocket.c').read_text()
http=(root/'main/moonraker.c').read_text()
with tempfile.TemporaryDirectory(prefix='estop-transport-') as tmp:
    folder=Path(tmp)
    (folder/'actual_estop_transport.c').write_text('\n'.join(function(ws,n) for n in ['append_command_text','append_command_json_string','moonraker_live_websocket_send_gcode'])+'\n'+function(http,'moonraker_send_gcode_script'))
    executable=folder/'test'
    subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wrestrict','-I',str(folder),'-I',str(root/'main'),str(root/'tools/audit/estop_transport_test.c'),'-o',str(executable)],check=True)
    subprocess.run([str(executable)],check=True)
