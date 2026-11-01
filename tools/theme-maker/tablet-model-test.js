'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),T=require('./tablet-model'),M=require('./theme-model'),Scene=require('./studio-scene');
let s=T.create();const start=JSON.stringify(s);let p=T.dispatch(s,'pause');assert.equal(JSON.stringify(s),start);assert.equal(p.print,'paused');assert.equal(T.dispatch(p,'pause').print,'printing');
p=T.dispatch(s,'stop');assert(p.modal);assert.equal(T.dispatch(p,'close').print,'printing');assert.equal(T.dispatch(p,'confirm-stop').print,'cancelled');
assert.equal(T.dispatch(T.dispatch(s,'estop'),'confirm-restart').print,'standby');
p=T.dispatch(s,'temperature:nozzle');p=T.dispatch(p,'field','220');p=T.dispatch(p,'set-number:nozzle');assert.equal(p.nozzle,220);assert(!p.modal);
p=T.dispatch(s,'temperature:bed');p=T.dispatch(p,'field','999');p=T.dispatch(p,'set-number:bed');assert(p.modal&&p.message);assert.equal(p.bed,60);
p=T.dispatch(s,'toolhead');p=T.dispatch(p,'move:0:1');assert.equal(p.position[0],111);assert.equal(T.dispatch(p,'step').step,10);
p=T.dispatch(s,'files-search');p=T.dispatch(p,'field','cube');p=T.dispatch(p,'files-search-apply');assert.deepEqual(T.visibleFiles(p).map(f=>f.name),['Calibration cube.gcode']);
p=T.dispatch(T.enter(s,'files'),'folder');assert.equal(T.visibleFiles(p).length,1);p=T.dispatch(p,'up');assert.equal(T.visibleFiles(p).length,6);
p=T.dispatch(T.enter(s,'files'),'file:1');p=T.dispatch(p,'file-print');assert(p.modal);p=T.dispatch(p,'confirm-print');assert.equal(p.page,'dashboard');assert.equal(p.job,'Tool tray.gcode');assert.equal(p.progress,0);
p=T.dispatch(s,'dry:Hold');assert(p.dryHeld);p=T.dispatch(p,'dry:Resume');assert(!p.dryHeld);p=T.dispatch(p,'dry:PLA');assert.equal(p.dryTarget,45);p=T.dispatch(p,'dry:Stop');assert.equal(p.dryTarget,0);
p=T.dispatch(s,'camera-configure');p=T.dispatch(p,'field','http://never-contact.example/stream');p=T.dispatch(p,'camera-save');assert(p.cameraConfigured);assert.equal(T.dispatch(p,'camera-next').camera,2);
p=T.dispatch(s,'command');p=T.dispatch(p,'field','G28');p=T.dispatch(p,'command-apply');assert(T.consoleRows(p).some(r=>r[1]==='G28'));assert.equal(T.consoleRows(T.dispatch(p,'temps')).length,T.consoleRows(p).length-1);assert.equal(T.consoleRows(T.dispatch(p,'clear-console')).length,0);
p=T.dispatch(T.enter(s,'telemetry'),'hold');assert(p.held);assert.equal(T.dispatch(p,'telemetry:Motion').telemetry,'Thermal');assert.equal(T.dispatch(p,'range').range,2);p=T.dispatch(p,'hold');assert.equal(T.dispatch(p,'telemetry:Motion').telemetry,'Motion');
p=T.dispatch(s,'setting:backup');p=T.dispatch(p,'backup:Restore encrypted');p=T.dispatch(p,'field','wrong');p=T.dispatch(p,'backup-complete:Restore encrypted');assert(p.message.includes('Password mismatch'));p=T.dispatch(p,'field','studio');p=T.dispatch(p,'backup-complete:Restore encrypted');assert.equal(p.modal.title,'Completed');assert.equal(p.field,'');
p=T.dispatch(s,'files-search');p=T.dispatch(p,'field',{text:'abcd',start:1,end:3});p=T.dispatch(p,'key','x');assert.equal(p.field,'axd');p=T.dispatch(p,'key','⌫');assert.equal(p.field,'ad');p=T.dispatch(p,'key','⇧');p=T.dispatch(p,'key','z');assert.equal(p.field,'aZd');
for(const page of T.pages){p=T.enter(s,page);p=T.dispatch(p,'scroll',1e6);assert(p.scroll<=T.scrollMax(p));p=T.dispatch(p,'scroll',-1e6);assert.equal(p.scroll,0);}
assert.deepEqual(T.dispatch(s,'unknown'),s);
const project=M.create('studio'),before=M.serialize(project);
for(const page of T.pages){const svg=Scene.render(project,page,{state:T.enter(s,page)});assert(!svg.includes('undefined'));assert(svg.includes('data-tablet-action'));for(const token of svg.matchAll(/data-theme-(?:ink|fill|edge)="([^"]+)"/g))assert(Object.hasOwn(project.palette,token[1]));}
const attack=T.dispatch(T.dispatch(s,'command'),'field','<script>alert(1)</script>');assert(Scene.render(project,'console',{state:attack}).includes('&lt;script&gt;'));assert.equal(M.serialize(project),before);
// Check copied page recipes against the actual native Studio builders, not screenshots of a generic design.
const root=path.resolve(__dirname,'../..'),source=f=>fs.readFileSync(path.join(root,'main',f),'utf8');
assert(source('ui_camera.c').includes('s_configure_button,100,356'));assert(Scene.render(project,'camera').includes('x="100" y="356"'));
for(const name of ['Calibration','Bed mesh','Devices','Macros']){assert(source('ui_tools.c').includes('"'+name+'"'));assert(Scene.render(project,'tools').includes(name));}
const settingsSource=source('ui_settings.c');assert(settingsSource.includes('sections[]={display,operator_section,connections,time_region,storage,firmware,setup,device,system,about}'));
for(const [title,rows]of T.settings){assert(settingsSource.includes('"'+title+'"'));for(const row of rows)assert(settingsSource.includes('"'+row[0]+'"'));}
for(const name of ['Toolhead control','Home','Pause','Resume','Object','Stop'])assert(source('ui_printer_actions.c').includes('"'+name+'"'));
assert(!Scene.render(project,'printer').includes('Motors off'));
console.log('PASS: native page recipes, immutable offline state, printer/temperature actions, file browsing/print confirmation, drybox programs, camera configuration, console/filtering, graph controls, password mismatch, cursor-aware keyboard, scroll bounds and escaped text');
p=T.dispatch(s,'toolhead');p=T.dispatch(p,'toolhead-tab:2');p=T.dispatch(p,'offset:-0.025');assert.equal(p.zOffset,-0.025);assert(Scene.render(project,'printer',{state:p}).includes('Runtime Z offset'));p=T.dispatch(p,'offset:reset');assert.equal(p.zOffset,0);p=T.dispatch(p,'toolhead-tab:1');p=T.dispatch(p,'extrusion-step:25');assert.equal(p.extrusionStep,25);assert(Scene.render(project,'printer',{state:p}).includes('Extrusion direction'));p=T.dispatch(p,'z-step:0.05');assert(Math.abs(T.dispatch(p,'move:2:1').position[2]-12.85)<1e-9);
console.log('PASS: native Toolhead tabs, independent Z/extrusion steps, runtime offset and reset');
