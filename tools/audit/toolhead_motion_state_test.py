#!/usr/bin/env python3
"""Exercise production motion formatters against parsing-state semantics and demo dispatch."""
import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, default=root / 'main/ui_printer_toolhead.c')
args = parser.parse_args()
source = args.source.read_text()


def function(name):
    start = source.index('bool ' + name + '(')
    return source[start:source.index('\n}\n', start) + 3]


# This fixture models the documented SAVE/RESTORE behavior. It does not run
# Klipper, its motion planner, user macros or physical endstop/heater checks.
class Machine:
    def __init__(self, xyz_absolute, e_absolute):
        self.xyz_absolute = xyz_absolute
        self.e_absolute = e_absolute
        self.position = dict(X=120., Y=140., Z=80., E=173.)
        self.base = dict(X=3., Y=4., Z=.2, E=73.)
        self.speed = 31.
        self.speed_factor = .75
        self.extrude_factor = 1.12
        self.saved = {'PAUSE_STATE': {'untouched': True}, 'default': {'untouched': True}}
        self.moves = []

    def parsing(self):
        return (self.xyz_absolute, self.e_absolute, self.base.copy(), self.speed,
                self.speed_factor, self.extrude_factor,
                self.position['E'] - self.base['E'])

    def run(self, script):
        for line in script.splitlines():
            command, *words = line.split()
            if command in ('SAVE_GCODE_STATE', 'RESTORE_GCODE_STATE'):
                values = dict(word.split('=', 1) for word in words)
                name = values['NAME']
                if command == 'SAVE_GCODE_STATE':
                    self.saved[name] = (self.parsing(), self.position.copy())
                else:
                    assert values.get('MOVE', '0') == '0', 'Manual motion must not move back'
                    state, position = self.saved[name]
                    (self.xyz_absolute, self.e_absolute, base, self.speed,
                     self.speed_factor, self.extrude_factor, _) = state
                    self.base = base.copy()
                    self.base['E'] += self.position['E'] - position['E']
            elif command == 'G91':
                self.xyz_absolute = False
            elif command == 'G90':
                self.xyz_absolute = True
            elif command == 'M83':
                self.e_absolute = False
            elif command == 'G1':
                arguments = {word[0]: float(word[1:]) for word in words}
                for axis in 'XYZ':
                    if axis in arguments:
                        self.position[axis] = (arguments[axis] + self.base[axis]
                                               if self.xyz_absolute else
                                               self.position[axis] + arguments[axis])
                if 'E' in arguments:
                    amount = arguments['E'] * self.extrude_factor
                    self.position['E'] = (amount + self.base['E']
                                          if self.xyz_absolute and self.e_absolute else
                                          self.position['E'] + amount)
                if 'F' in arguments:
                    self.speed = arguments['F'] / 60 * self.speed_factor
                self.moves.append(self.position.copy())
            else:
                raise AssertionError('Unexpected command: ' + line)


with tempfile.TemporaryDirectory(prefix='toolhead-state-') as directory:
    tmp = Path(directory)
    emitted = tmp / 'format.c'
    emitted.write_text('''#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
''' + function('ui_printer_toolhead_format_jog_command') + '\n' +
        function('format_extrusion_distance') + r'''
int main(void) {
    char buffer[160];
    const char *axes[] = {"X+","X-","Y+","Y-","Z+","Z-"};
    for (unsigned i=0;i<6;i++) {
        double distance = i<4 ? 50 : 25;
        assert(ui_printer_toolhead_format_jog_command(axes[i],distance,buffer,sizeof(buffer)));
        size_t size=strlen(buffer)+1;
        assert(fwrite(buffer,1,size,stdout)==size);
        for(size_t limit=1;limit<size;limit++) {
            assert(!ui_printer_toolhead_format_jog_command(axes[i],distance,buffer,limit));
            assert(!buffer[0]);
        }
    }
    for (unsigned i=0;i<2;i++) {
        const char *direction=i ? "RETRACT" : "EXTRUDE";
        assert(format_extrusion_distance(direction,50,buffer,sizeof(buffer)));
        size_t size=strlen(buffer)+1;
        assert(fwrite(buffer,1,size,stdout)==size);
        for(size_t limit=1;limit<size;limit++) {
            assert(!format_extrusion_distance(direction,50,buffer,limit));
            assert(!buffer[0]);
        }
    }
    return 0;
}
''')
    exe = tmp / 'format'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                    str(emitted), '-lm', '-o', str(exe)], check=True)
    env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
    scripts = subprocess.check_output([str(exe)], env=env).decode().split('\0')[:-1]
    expected = [('X', 50.), ('X', -50.), ('Y', 50.), ('Y', -50.),
                ('Z', 25.), ('Z', -25.), ('E', 50.), ('E', -50.)]
    for script, (axis, distance) in zip(scripts, expected, strict=True):
        for xyz in (False, True):
            for extrude in (False, True):
                machine = Machine(xyz, extrude)
                before = machine.parsing()
                position = machine.position.copy()
                saved = copy.deepcopy(machine.saved)
                machine.run(script)
                after = machine.parsing()
                # E's physical offset advances while its logical coordinate is preserved.
                assert before[:2] == after[:2], (script, before, after)
                assert before[3:6] == after[3:6], 'Feedrate/overrides changed'
                assert abs(before[6] - after[6]) < 1.e-9, 'Logical E changed'
                for name in 'XYZ':
                    assert before[2][name] == after[2][name], 'Coordinate origin changed'
                for name in saved:
                    assert machine.saved[name] == saved[name], 'User saved state overwritten'
                assert len(machine.moves) == 1
                for name in 'XYZE':
                    delta = distance * (machine.extrude_factor if axis == 'E' else 1) if name == axis else 0
                    assert abs(machine.position[name] - position[name] - delta) < 1.e-9

    # Apply the same emitted scripts to the production demo, using real state types.
    demo = tmp / 'demo.c'
    lines = ['#include "printer_simulation.h"', '#include <assert.h>', '#include <string.h>',
             'int main(void){moonraker_state_t s={.moonraker_ok=true,.live_data_ok=true};',
             'bool excluded[2]={false,false},sensor=true;strcpy(s.printer_state,"standby");']
    for script, (axis, distance) in zip(scripts, expected, strict=True):
        lines.append('{ double x=s.toolhead_x,y=s.toolhead_y,z=s.toolhead_z;')
        lines.append('assert(printer_simulation_apply(&s,excluded,&sensor,' + json.dumps(script) + '));')
        for name in 'XYZ':
            delta = distance if axis == name else 0
            lines.append(f'assert(s.toolhead_{name.lower()}=={name.lower()}+({delta}));')
        # Incomplete restores and extra commands must be rejected without mutation.
        for bad in (script.rsplit('\n', 1)[0], script + '\nM112', script.replace('MOVE=0', 'MOVE=1')):
            lines.append('{moonraker_state_t before=s;assert(!printer_simulation_apply(&s,excluded,&sensor,' + json.dumps(bad) + '));assert(!memcmp(&s,&before,sizeof(s)));}')
        lines.append('}')
    lines.append('return 0;}')
    demo.write_text('\n'.join(lines) + '\n')
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    '-I', str(root/'main'), '-I', str(root/'tools/theme-maker/linux/native/shims'),
                    str(demo), str(root/'main/printer_simulation.c'), '-lm', '-o', str(tmp/'demo')], check=True)
    subprocess.run([str(tmp/'demo')], check=True)
print('PASS: emitted jog/extrusion scripts preserve modes/feedrate/origins/logical E and user saved states; one intended move, all short buffers refused, production demo parity and malformed-script rejection')
