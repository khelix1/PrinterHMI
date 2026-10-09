#!/usr/bin/env python3
"""Render actual Klipper Jinja macros and simulate state/command sequencing. Requires jinja2."""
from pathlib import Path
from types import SimpleNamespace as NS
import configparser, re
import jinja2
root=Path(__file__).resolve().parents[2]
config=configparser.ConfigParser(interpolation=None,strict=True)
config.read(root/'config/examples/sermoon_d1_filament_recovery.cfg')
env=jinja2.Environment(variable_start_string='{',variable_end_string='}')
class Rejected(Exception): pass
class Printer(dict):
    def __getattr__(self,k): return self[k]
class Machine:
    def __init__(self,z=100,absolute=True):
        self.p=Printer(pause_resume=NS(is_paused=False),extruder=NS(can_extrude=True,target=205,temperature=205),toolhead=NS(axis_maximum=NS(z=300),position=NS(x=120,y=140,z=z),homed_axes='xyz'),configfile=NS(settings=NS(extruder=NS(max_temp=260))))
        self.p['filament_switch_sensor filament_sensor']=NS(enabled=True,filament_detected=True)
        self.p['gcode_macro PAUSE']=NS(saved_temp=0,retracted=0)
        self.xyz_absolute=absolute;self.e_absolute=absolute;self.e=100;self.saved={};self.moves=[];self.heaters_on=True;self.motors_on=True
    def fail(self,message): raise Rejected(message)
    def run(self,name,**params):
        section='idle_timeout' if name=='IDLE' else 'gcode_macro '+name
        rendered=env.from_string(config[section]['gcode']).render(printer=self.p,params=params,action_raise_error=self.fail)
        for line in rendered.splitlines():
            line=line.strip()
            if not line or line.startswith('#'):continue
            command,*words=line.split();values=dict(w.split('=',1) for w in words if '=' in w)
            if 'gcode_macro '+command in config:self.run(command,**values);continue
            if command=='PAUSE_BASE':self.p.pause_resume.is_paused=True;self.saved['base']=self.snapshot()
            elif command=='RESUME_BASE':self.restore(self.saved['base'],move=True);self.p.pause_resume.is_paused=False
            elif command in ('CLEAR_PAUSE','CANCEL_PRINT_BASE'):self.p.pause_resume.is_paused=False
            elif command=='SET_GCODE_VARIABLE':setattr(self.p['gcode_macro '+values['MACRO']],values['VARIABLE'],float(values['VALUE']))
            elif command=='SAVE_GCODE_STATE':self.saved[values['NAME']]=self.snapshot()
            elif command=='RESTORE_GCODE_STATE':self.restore(self.saved[values['NAME']],move=values.get('MOVE','0')=='1')
            elif command=='M83':self.e_absolute=False
            elif command=='G90':self.xyz_absolute=True
            elif command=='G91':self.xyz_absolute=False
            elif command=='G1':
                axes={w[0]:float(w[1:]) for w in words}
                for axis in 'XYZ':
                    if axis in axes:
                        old=getattr(self.p.toolhead.position,axis.lower());new=axes[axis] if self.xyz_absolute else old+axes[axis]
                        assert axis!='Z' or new<=300
                        setattr(self.p.toolhead.position,axis.lower(),new)
                if 'E' in axes:self.e=axes['E'] if self.e_absolute else self.e+axes['E']
                self.moves.append(axes)
            elif command in ('M104','M109'):
                target=float(next(w[1:] for w in words if w.startswith('S')));self.p.extruder.target=target
                if command=='M109':self.p.extruder.temperature=target;self.p.extruder.can_extrude=target>=170
            elif command=='TURN_OFF_HEATERS':self.heaters_on=False;self.p.extruder.target=0
            elif command=='M84':self.motors_on=False;self.p.toolhead.homed_axes=''
            elif command in ('M400','M107','RESPOND'):pass
            else:raise AssertionError(line)
    def snapshot(self):return (vars(self.p.toolhead.position).copy(),self.e,self.xyz_absolute,self.e_absolute)
    def restore(self,s,move=False):
        position,e,xyz,ext=s
        if move:self.p.toolhead.position=NS(**position)
        self.e=e;self.xyz_absolute=xyz;self.e_absolute=ext

def reject(m,name,**params):
    before=len(m.moves)
    try:m.run(name,**params)
    except Rejected:pass
    else:raise AssertionError('Should reject '+name)
    assert len(m.moves)==before
for absolute in (False,True):
    for z in (100,299,300):
        m=Machine(z,absolute);start=m.snapshot();m.run('PAUSE');assert m.p.toolhead.position.z==min(z+10,300);assert (m.p.toolhead.position.x,m.p.toolhead.position.y)==(2,10)
        assert (m.xyz_absolute,m.e_absolute)==(absolute,absolute)
        saved=m.p['gcode_macro PAUSE'].saved_temp;count=len(m.moves);m.run('PAUSE');assert len(m.moves)==count and m.p['gcode_macro PAUSE'].saved_temp==saved
        m.run('IDLE');assert m.motors_on and m.heaters_on and m.p.extruder.target==0
        m.p.extruder.temperature=40;m.p.extruder.can_extrude=False;reject(m,'HMI_FILAMENT_LOAD')
        m.run('HMI_FILAMENT_HEAT',TEMP=240);assert m.p.extruder.target==240 and m.p['gcode_macro PAUSE'].saved_temp==240
        reject(m,'HMI_FILAMENT_HEAT',TEMP=260);reject(m,'HMI_FILAMENT_HEAT',TEMP=100)
        m.p.extruder.temperature=240;m.p.extruder.can_extrude=True
        for name,amount in [('HMI_FILAMENT_UNLOAD',-80),('HMI_FILAMENT_LOAD',30),('HMI_FILAMENT_PURGE',10)]:
            m.run(name);assert m.moves[-1]['E']==amount;assert (m.xyz_absolute,m.e_absolute)==(absolute,absolute)
        m.p['filament_switch_sensor filament_sensor'].filament_detected=False;reject(m,'RESUME')
        m.p['filament_switch_sensor filament_sensor'].filament_detected=True;m.p.extruder.can_extrude=False;m.p.extruder.temperature=40
        m.run('RESUME');assert not m.p.pause_resume.is_paused and m.p.extruder.temperature==240;assert m.snapshot()==start
m=Machine();reject(m,'HMI_FILAMENT_LOAD');m.run('PAUSE');m.p.toolhead.homed_axes='xy';reject(m,'RESUME');reject(m,'HMI_FILAMENT_UNLOAD')
m=Machine(299);m.run('PAUSE');m.p.extruder.can_extrude=False;m.p.extruder.temperature=40;m.run('CANCEL_PRINT');assert not m.p.pause_resume.is_paused and not m.heaters_on
m=Machine();m.run('IDLE');assert not m.motors_on and not m.heaters_on
print('PASS: actual macro templates, capped lift, repeated pause, both extrusion modes, cooling/reheat, sensor/homing guards, load/unload/purge and cold cancel')
