#!/usr/bin/env python3
"""Run the portable simulation against actual firmware state types, with UBSan."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[3]
with tempfile.TemporaryDirectory() as temporary:
    source=Path(temporary)/'test.c';exe=Path(temporary)/'test'
    source.write_text(r'''
#include "printer_simulation.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void){
 moonraker_state_t state={.nozzle_temp=25,.bed_temp=25,.speed_factor=100,.flow_factor=100};
 bool excluded[2]={true,true},sensor=true;
 assert(printer_simulation_scenario(&state,"printing"));
 printer_simulation_advance(&state,10);assert(state.progress>.35&&state.nozzle_temp==55&&state.bed_temp==31);
 assert(printer_simulation_apply(&state,excluded,&sensor,"PAUSE"));double progress=state.progress,duration=state.print_duration;
 printer_simulation_advance(&state,100);assert(state.progress==progress&&state.print_duration==duration&&state.live_velocity==0);
 assert(printer_simulation_apply(&state,excluded,&sensor,"RESUME"));printer_simulation_advance(&state,1800);assert(!strcmp(state.printer_state,"complete")&&state.progress==1&&state.nozzle_target==0);
 assert(printer_simulation_apply(&state,excluded,&sensor,"SDCARD_PRINT_FILE FILENAME=\"demo.gcode\""));assert(!strcmp(state.printer_file,"demo.gcode")&&state.progress==0&&!excluded[0]&&!excluded[1]);
 assert(!printer_simulation_apply(&state,excluded,&sensor,"M104 Snan"));assert(!printer_simulation_apply(&state,excluded,&sensor,"M140 S9999"));assert(state.nozzle_target==210&&state.bed_target==60);
 assert(printer_simulation_apply(&state,excluded,&sensor,"M104 S210"));assert(printer_simulation_apply(&state,excluded,&sensor,"M112"));assert(!strcmp(state.printer_state,"shutdown")&&state.nozzle_target==0);
 assert(printer_simulation_scenario(&state,"offline"));progress=state.progress;printer_simulation_advance(&state,3600);assert(state.progress==progress);
 assert(!printer_simulation_scenario(&state,"unknown"));printer_simulation_advance(&state,NAN);printer_simulation_advance(&state,-1);assert(state.progress==progress);
 assert(printer_simulation_scenario(&state,"idle"));assert(!state.printer_file[0]&&state.progress==0);
 return 0;
}
''')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I',str(root/'main'),'-I',str(root/'main'),'-I',str(root/'tools/theme-maker/linux/native/shims'),str(source),str(root/'main/printer_simulation.c'),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print('PASS: shared simulation heating, progress, pause/resume, completion, new jobs, emergency stop, offline and input bounds')
