'use strict';
const assert=require('node:assert/strict'),M=require('./theme-model'),T=require('./tablet-model'),S=require('./studio-scene');
const theme=M.create('studio');const actions=(page,state=T.create())=>new Set([...S.render(theme,page,{state}).matchAll(/data-tablet-action="([^"]+)"/g)].map(m=>m[1]));
const dash=actions('dashboard');for(const action of ['temperature:nozzle','temperature:bed','tune','fan','tool:Devices','nav:drybox'])assert(!dash.has(action),'Dashboard reading must be inert: '+action);
for(const action of ['printers','estop','fullscreen','pause','stop','object'])assert(dash.has(action));
const printer=actions('printer');for(const action of ['temperature:nozzle','temperature:bed','fan','tune','toolhead','home'])assert(printer.has(action));assert(!printer.has('dry-target'));
assert(!actions('drybox').has('dry-target'));assert(!actions('settings').has('setting:brightness'));
const paused=T.dispatch(T.create(),'pause');assert(actions('printer',paused).has('pause'));const stopped=T.dispatch(T.create(),'confirm-stop');for(const page of ['dashboard','printer'])for(const action of ['pause','stop','object'])assert(!actions(page,stopped).has(action),page+' disabled '+action);
const inertSvg=S.render(theme,'dashboard',{state:stopped});assert(inertSvg.includes('aria-disabled="true"'));assert(!inertSvg.match(/data-tablet-action="pause"/));
// These native Studio builders contain no event registration for their readings.
const fs=require('node:fs'),path=require('node:path'),root=path.resolve(__dirname,'../..');const machine=fs.readFileSync(path.join(root,'main/ui_machine_status.c'),'utf8').split('static lv_obj_t *studio_machine_create')[1].split('\n}\n')[0];assert(!machine.includes('LV_EVENT_CLICKED'));const dry=fs.readFileSync(path.join(root,'main/ui_drybox_page.c'),'utf8').split('static void studio_drybox_create')[1].split('\n}\n')[0];assert(!dry.includes('target_label,LV_EVENT_CLICKED'));
console.log('PASS: Dashboard and Drybox readings stay inert, native Printer controls remain active, disabled print actions have no click/keyboard target, brightness is slider-only');
