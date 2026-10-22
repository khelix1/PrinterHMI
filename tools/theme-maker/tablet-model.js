/* Local tablet simulation. No I/O, printer transport, secrets or project writes. */
(function(g){'use strict';
const pages=['dashboard','printer','files','camera','tools','console','drybox','settings','network','telemetry'];
const copy=v=>JSON.parse(JSON.stringify(v));
const files=[['Spool holder.gcode',1.4,128],['Tool tray.gcode',2.2,210],['Cable organizer.gcode',0.8,70],['Calibration cube.gcode',0.2,26],['Planter.gcode',3.1,240],['Bracket.gcode',1.1,90]];
const settings=[
 ['Display & appearance',[
 ['Brightness','Display backlight level','75%','brightness'],['Sleep timeout','Tap to change display sleep behavior','OFF','sleep'],['Theme','Tap to change the operator interface appearance','Studio','theme'],['Accent color','Override the theme accent while preserving semantic colors','Default','accent'],['Display density','Choose compact, comfortable, or spacious type and spacing','Comfortable','density'],['Accessibility','Text, contrast, transparency, and motion preferences','Standard','accessibility']]],
 ['Notifications & history',[['Notifications','System-wide level; faults remain visible','All','notifications'],['Event history','Recent connection, firmware, and machine events','View history','history',true]]],
 ['Connections',[['Network & Moonraker','Wi-Fi, active printer connection and discovery settings','Open network','network',true]]],
 ['Time & region',[['Time zone','Tap to change local time and daylight-saving rules','America/Chicago','timezone']]],
 ['Storage & backup',[['SD Card','Removable storage status','Mounted',''],['Storage capacity','','8.2 GB / 32 GB',''],['Configuration backup','Back up or restore profiles and interface settings','Backup / restore','backup',true]]],
 ['Firmware & updates',[['Firmware version','Version embedded in the running OTA image','7.0.0',''],['Image build','Compilation date and time of the running image','Demo image',''],['Firmware channel','Build channel of the running image','Stable',''],['Source revision','Source commit used to build this image','Preview sample',''],['Firmware update','Check for and install an OTA update','OTA update','ota',true]]],
 ['Setup center',[['Run system setup again','Guided Wi-Fi, printer, and camera configuration','Open setup','setup',true]]],
 ['Maintenance',[['Reboot controller','Restart the ESP32-P4 controller','Reboot','reboot',true],['Restart Klipper','Restart the active printer firmware','Restart Klipper','restart',true],['Factory reset','Erase saved connections and preferences','Factory reset','reset',true]]],
 ['System information',[['Uptime','Time since controller startup','02:14:36',''],['Internal RAM','Free now / Lowest / Largest block','280 / 250 / 128 KB',''],['PSRAM','Free now / Lowest','27.3 / 25.4 MB',''],['ESP-IDF version','Framework used to build the running image','v6.0.2','']]],
 ['About',[['About PrinterHMI','Product information, software credits and acknowledgements','View about','about',true]]]
];
function create(){return {page:'dashboard',printer:'Sermoon D1',print:'printing',job:'Spool holder.gcode',progress:64,nozzle:210,bed:60,speed:100,flow:100,fan:65,position:[110,110,12.8],step:1,zStep:0.1,extrusionStep:5,zOffset:0,toolheadTab:0,historyCursor:-1,cameraRotation:0,cameraMirrorH:false,cameraMirrorV:false,accessibility:{},excluded:[],dryProgram:'PETG',dryHeld:false,dryTarget:55,camera:1,cameraFit:'Contain',cameraConfigured:false,fullscreen:false,modal:null,field:'',message:'',query:'',folder:'',sort:0,selected:null,scroll:0,telemetry:'Thermal',range:2,targets:true,held:false,consoleFilter:'All entries',consoleQuery:'',hideTemps:false,follow:true,log:[['system','Klipper state: Ready'],['response','Printer is ready'],['temperature','T:210.0 /210.0 B:60.0 /60.0']],wifi:'Studio Network',preferences:{},shift:false,numeric:false,caret:null,anchor:null};}
function enter(state,page){const n=copy(state);if(!pages.includes(page))return n;if(n.page!==page){n.page=page;n.scroll=0;n.modal=null;n.fullscreen=false;n.message='';}return n;}
function modal(s,title,body='',choices=[],input=false,submit=''){s.modal={title,body,choices,input,submit};s.field='';s.message='';s.shift=false;s.numeric=false;s.caret=null;s.anchor=null;}
function choices(s,title,items,prefix){modal(s,title,'',items.map(v=>[v,prefix+v]));}
function visibleFiles(s){return files.map((f,i)=>({name:f[0],size:f[1],minutes:f[2],index:i})).filter(f=>(!s.folder||f.name==='Calibration cube.gcode')&&f.name.toLowerCase().includes(s.query.toLowerCase())).sort((a,b)=>s.sort===1?b.index-a.index:s.sort===2?b.size-a.size:a.name.localeCompare(b.name));}
function transition(state,action,value){let n=copy(state);if(typeof action!=='string'||action.length>256)return n;
 if(action==='close'){n.modal=null;n.fullscreen=false;n.message='';return n;}
 if(action==='field'){n.field=String(value?.text??value??'').slice(0,160);n.caret=Math.max(0,Math.min(n.field.length,Number.isInteger(value?.start)?value.start:n.field.length));n.anchor=Math.max(n.caret,Math.min(n.field.length,Number.isInteger(value?.end)?value.end:n.caret));return n;}
 if(action==='key'){const k=String(value??''),start=n.caret===null?n.field.length:n.caret,end=n.anchor===null?start:n.anchor;
 if(k==='ABC')n.numeric=false;else if(k==='123')n.numeric=true;else if(k==='⇧')n.shift=!n.shift;
 else if(k==='←'||k==='→'){n.caret=n.anchor=Math.max(0,Math.min(n.field.length,start+(k==='←'?-1:1)));}
 else if(k==='+/-'){if(!n.modal?.returnControl){n.field=n.field.startsWith('-')?n.field.slice(1):'-'+n.field;n.caret=n.anchor=n.field.length;}}
 else if(n.modal?.returnControl&&k.length===1&&!/^[0-9]$/.test(k))return n;
 else if(k==='⌫'){const left=start===end?Math.max(0,start-1):start;n.field=n.field.slice(0,left)+n.field.slice(end);n.caret=n.anchor=left;}
 else if(k.length===1){const v=n.shift?k.toUpperCase():k;n.field=(n.field.slice(0,start)+v+n.field.slice(end)).slice(0,160);n.caret=n.anchor=Math.min(n.field.length,start+1);}return n;}
 if(action==='scroll'){n.scroll=Math.max(0,Math.min(scrollMax(n),n.scroll+(Number(value)||0)));if(n.page==='console'&&n.scroll<scrollMax(n))n.follow=false;return n;}
 if(action.startsWith('nav:'))return enter(n,action.slice(4));
 if(action==='pause'){if(n.print==='printing')n.print='paused';else if(n.print==='paused')n.print='printing';return n;}
 if(action==='stop'){modal(n,'Stop print?','This will stop the active print job.',[['Back','close'],['Stop print','confirm-stop']]);return n;}
 if(action==='confirm-stop'){n.print='cancelled';n.modal=null;return n;}
 if(action==='estop'){modal(n,'Stop request sent','Offline simulation: the printer is now stopped.',[['Restart Klipper','confirm-restart'],['Close','close']]);n.print='stopped';return n;}
 if(action==='confirm-restart'){n.print='standby';n.nozzle=0;n.bed=0;n.modal=null;return n;}
 if(action==='object'){choices(n,'Cancel object',['Spool holder','Bracket'].filter(v=>!n.excluded.includes(v)),'object:');return n;}
 if(action.startsWith('object:')){const name=action.slice(7);if(!['Spool holder','Bracket'].includes(name))return n;modal(n,'Cancel object?','Cancel '+name+'? The remaining objects will continue printing.',[['Back','object'],['Cancel object','exclude:'+name]]);return n;}
 if(action.startsWith('exclude:')){const name=action.slice(8);if(['Spool holder','Bracket'].includes(name)&&!n.excluded.includes(name))n.excluded.push(name);return dispatch(n,'object');}
 if(action==='home'){modal(n,'Home all axes?','The toolhead will move to its home position.',[['Cancel','close'],['Home','confirm-home']]);return n;}
 if(action==='confirm-home'){n.position=[0,0,0];if(n.modal?.title!=='Toolhead control')n.modal=null;return n;}
 if(action.startsWith('toolhead-tab:')){const v=Number(action.slice(13));if([0,1,2].includes(v))n.toolheadTab=v;return n;}
 if(action.startsWith('xy-step:')){const v=Number(action.slice(8));if([1,10,50].includes(v))n.step=v;return n;}
 if(action.startsWith('z-step:')){const v=Number(action.slice(7));if([0.01,0.05,0.1,0.5,1,5,10,25].includes(v))n.zStep=v;return n;}
 if(action.startsWith('extrusion-step:')){const v=Number(action.slice(15));if([1,5,10,25,50].includes(v))n.extrusionStep=v;return n;}
 if(action.startsWith('offset:')){const v=action.slice(7);if(v==='reset')n.zOffset=0;else if(Number.isFinite(Number(v)))n.zOffset=Math.max(-5,Math.min(5,n.zOffset+Number(v)));return n;}
 if(action==='command-prev'||action==='command-next'){const entries=n.log.filter(r=>r[0]==='command').map(r=>r[1]);n.historyCursor=Math.max(-1,Math.min(entries.length-1,n.historyCursor+(action==='command-prev'?1:-1)));n.field=n.historyCursor<0?'':entries[entries.length-1-n.historyCursor]||'';n.caret=n.anchor=null;return n;}
 if(action==='toolhead'){modal(n,'Toolhead control','Choose an axis, step size or extrusion action.',[['X −','move:0:-1'],['X +','move:0:1'],['Y −','move:1:-1'],['Y +','move:1:1'],['Z −','move:2:-1'],['Z +','move:2:1'],['Step '+n.step+' mm','step'],['Extrude 5 mm','extrude'],['Retract 5 mm','retract'],['Nozzle temperature','temperature:nozzle'],['Bed temperature','temperature:bed'],['Part fan','fan']]);return n;}
 if(action.startsWith('move:')){const [,i,d]=action.split(':');if(['0','1','2'].includes(i)&&['-1','1'].includes(d))n.position[+i]=Math.max(0,Math.min(300,n.position[+i]+Number(d)*(i==='2'?n.zStep:n.step)));return n;}
 if(action==='step'){n.step=[0.01,0.1,1,10][([0.01,0.1,1,10].indexOf(n.step)+1)%4];n=dispatch(n,'toolhead');return n;}
 if(action==='extrude'||action==='retract'){n.message=(n.nozzle<170?'Heat the nozzle before extrusion.':'Simulated '+action+' complete.');return n;}
 if(action==='tune'){modal(n,'Print tuning');return n;}
 if(action.startsWith('tuning:')){const key=action.slice(7),v=Number(value);if(['speed','flow'].includes(key)&&Number.isFinite(v))n[key]=Math.max(50,Math.min(150,Math.round(v)));return n;}
 if(action.startsWith('tune:')){const key=action.includes('Speed')?'speed':'flow';modal(n,action.slice(5),'Enter a percentage (10–300).',[],true,'set-number:'+key);n.field=String(n[key]);n.numeric=true;return n;}
 if(action.startsWith('temperature:')||action==='fan'||action==='dry-target'){
 const key=action==='fan'?'fan':action==='dry-target'?'dryTarget':action.slice(12);if(!['nozzle','bed','fan','dryTarget'].includes(key))return n;
 if(key==='dryTarget'){modal(n,'Target temperature','Enter 0–80 C.',[],true,'set-number:'+key);n.field=String(n[key]);n.numeric=true;return n;}
 modal(n,key==='fan'?'Part cooling fan':key==='bed'?'Bed temperature':'Nozzle temperature');n.modal.control=key;return n;
 }
 if(action.startsWith('preset:')){const [,key,v]=action.split(':'),allowed={nozzle:[180,200,215,230,250,0],bed:[50,60,70,80,100,0],fan:[0,25,50,75,100]};if(allowed[key]?.includes(Number(v))){n[key]=Number(v);n.modal=null;}return n;}
 if(action.startsWith('custom:')){const key=action.slice(7);if(!['nozzle','bed'].includes(key))return n;modal(n,'Custom '+key+' temperature','Enter 0–'+(key==='nozzle'?300:120)+' C (0 = off)',[],true,'set-number:'+key);n.modal.returnControl=key;n.field=String(n[key]);n.numeric=true;return n;}
 if(action==='back-control'){const key=n.modal?.returnControl;return key?dispatch(n,'temperature:'+key):dispatch(n,'close');}
  if(action.startsWith('set-number:')){const key=action.slice(11),limits={nozzle:[0,300],bed:[0,120],fan:[0,100],dryTarget:[0,80],speed:[10,300],flow:[10,300],brightness:[10,100]};const v=Number(n.field);if(!limits[key]||!n.field.trim()||!Number.isFinite(v)||v<limits[key][0]||v>limits[key][1]){n.message='Enter a value within the displayed range.';return n;}if(key==='brightness')n.preferences.brightness=v+'%';else n[key]=v;const parent=n.modal?.returnControl;if(parent)return dispatch(n,'temperature:'+parent);n.modal=null;return n;}
 if(action==='files-search'||action==='console-search'||action==='command'){modal(n,action==='files-search'?'Search files':action==='console-search'?'Search console':'Send command',action==='command'?'Offline simulation: commands are recorded locally.':'Enter text to match.',[],true,action+'-apply');n.field=action==='files-search'?n.query:action==='console-search'?n.consoleQuery:'';return n;}
 if(action==='files-search-apply'){n.query=n.field;n.scroll=0;n.selected=null;n.modal=null;return n;}
 if(action==='console-search-apply'){n.consoleQuery=n.field;n.scroll=0;n.modal=null;return n;}
 if(action==='command-apply'){if(!n.field.trim()){n.message='Enter a command before sending.';return n;}n.log.push(['command',n.field.trim()],['response','ok · simulated']);n.log=n.log.slice(-80);if(n.follow)n.scroll=scrollMax(n);n.modal=null;return n;}
 if(action==='refresh-files'){n.message='File list refreshed';return n;}
 if(action==='sort'){n.sort=(n.sort+1)%3;n.scroll=0;return n;}
 if(action==='up'){n.folder='';n.query='';n.scroll=0;n.selected=null;return n;}
 if(action==='folder'){n.folder='Calibration';n.scroll=0;n.selected=null;return n;}
 if(action.startsWith('file:')){const i=Number(action.slice(5));if(Number.isInteger(i)&&i>=0&&i<files.length)n.selected=i;return n;}
 if(action==='file-cancel'){n.selected=null;return n;}
 if(action==='file-details'&&n.selected!==null){const f=files[n.selected];modal(n,'File details',f[0]+'\nLayer height: 0.20 mm\nFilament: PLA · 12.5 m\nEstimated time: '+f[2]+' minutes\nSize: '+f[1]+' MB',[['Close','close']]);return n;}
 if(action==='file-print'&&n.selected!==null){modal(n,'Start print?','Start '+files[n.selected][0]+' on '+n.printer+'?',[['Cancel','close'],['Print','confirm-print']]);return n;}
 if(action==='confirm-print'&&n.selected!==null){n.job=files[n.selected][0];n.print='printing';n.progress=0;n.modal=null;return enter(n,'dashboard');}
 if(action==='fullscreen'){n.fullscreen=true;return n;}
 if(action.startsWith('dry:')){const v=action.slice(4);if(v==='Stop'){n.dryProgram='No program';n.dryTarget=0;n.dryHeld=false;}else if(v==='Hold')n.dryHeld=true;else if(v==='Resume')n.dryHeld=false;else if(['PLA','PETG'].includes(v)){n.dryProgram=v;n.dryTarget=v==='PLA'?45:55;n.dryHeld=false;}return n;}
 if(action==='camera-next'){n.camera=n.camera===1?2:1;return n;}
 if(action==='camera-configure'){modal(n,'Camera configuration','Enter a sample camera URL. It will not be contacted.',[],true,'camera-save');return n;}
 if(action==='camera-save'){n.cameraConfigured=!!n.field.trim();n.modal=null;return n;}
 if(action==='camera-view'){modal(n,'Camera view','Camera '+n.camera);return n;}
 if(action.startsWith('camera-rotation:')){const v=Number(action.slice(16));if([0,90,180,270].includes(v))n.cameraRotation=v;return n;}
 if(action==='camera-mirror-h'){n.cameraMirrorH=!n.cameraMirrorH;return n;}
 if(action==='camera-mirror-v'){n.cameraMirrorV=!n.cameraMirrorV;return n;}
 if(action==='camera-reset'){n.cameraRotation=0;n.cameraMirrorH=n.cameraMirrorV=false;return n;}
 if(action.startsWith('camera-fit:')){n.cameraFit=action.slice(11);n.modal=null;return n;}
 if(action==='console-filter'){choices(n,'Console filter',['All entries','Errors + warnings','Errors','Warnings','Commands','Responses','System'],'console-filter:');return n;}
 if(action.startsWith('console-filter:')){n.consoleFilter=action.slice(15);n.modal=null;return n;}
 if(action==='temps'){n.hideTemps=!n.hideTemps;return n;}
 if(action==='follow'){n.follow=!n.follow;if(n.follow)n.scroll=scrollMax(n);return n;}
 if(action==='clear-console'){n.log=[];n.scroll=0;return n;}
 if(action==='reset-filters'){n.consoleQuery='';n.consoleFilter='All entries';n.hideTemps=false;return n;}
 if(action.startsWith('telemetry:')&&!n.held){const v=action.slice(10);if(['Thermal','Motion','Drybox'].includes(v)){n.telemetry=v;n.scroll=0;}return n;}
 if(action==='hold'){n.held=!n.held;return n;}
 if(action==='targets'&&!n.held){n.targets=!n.targets;return n;}
 if(action==='range'&&!n.held){n.range=n.range===2?5:n.range===5?10:2;return n;}
 if(action==='hotend'&&!n.held){choices(n,'Hotend',['Nozzle'],'hotend-select:');return n;}
 if(action==='hotend-select:Nozzle'){n.modal=null;return n;}
 if(action==='printers'){choices(n,'Choose printer',['Sermoon D1','Sermoon D1 #2'],'printer:');return n;}
 if(action.startsWith('printer:')){n.printer=action.slice(8);n.modal=null;return n;}
 if(action==='scan'){n.message='Scan complete · 3 networks found';return n;}
 if(action.startsWith('wifi:')){modal(n,'Connect to '+action.slice(5),'Enter a sample password; nothing is stored or transmitted.',[],true,'wifi-save:'+action.slice(5));return n;}
 if(action.startsWith('wifi-save:')){n.wifi=action.slice(10);n.field='';n.modal=null;return n;}
 if(action==='tool:Devices'){modal(n,'Devices','Nozzle 210 C · Bed 60 C\nPart fan 65% · Filament present',[['Graphs','nav:telemetry'],['Endstops','endstops'],['Close','close']]);return n;}
 if(action==='endstops'){modal(n,'Live endstops','X: open\nY: open\nZ: open\nProbe: open',[['Refresh','endstops'],['Close','close']]);return n;}
 if(action==='tool:Calibration'){choices(n,'Calibration',['Probe / Z offset','Bed screws','Probe accuracy','Motion diagnostics'],'calibration:');return n;}
 if(action.startsWith('calibration:')){modal(n,action.slice(12),'Simulated machine workflow. No motion is sent.',[['Run test','calibration-run'],['Close','close']]);return n;}
 if(action==='calibration-run'){n.message='Simulated test complete. Results within tolerance.';return n;}
 if(action==='tool:Bed mesh'){modal(n,'Bed mesh','Profile: default\n5 × 5 sampled surface · range 0.12 mm',[['Profiles','mesh-profiles'],['Calibrate','calibration:Bed mesh'],['Close','close']]);return n;}
 if(action==='mesh-profiles'){choices(n,'Bed mesh profiles',['default','PLA','PETG'],'mesh:');return n;}
 if(action.startsWith('mesh:')){modal(n,'Mesh profile loaded',action.slice(5),[['Close','close']]);return n;}
 if(action==='tool:Macros'){choices(n,'Macros',['START_PRINT','PARK','LOAD_FILAMENT','UNLOAD_FILAMENT'],'macro:');return n;}
 if(action.startsWith('macro:')){modal(n,'Run macro?',action.slice(6),[['Cancel','close'],['Run','macro-run']]);return n;}
 if(action==='macro-run'){n.message='Macro completed in the offline simulation.';return n;}
 if(action.startsWith('setting:')){const k=action.slice(8);if(k==='network')return enter(n,'network');
 if(k==='accessibility'){modal(n,'Accessibility','Tap an option to toggle it.');return n;}
 const sets={sleep:['OFF','5 MIN','15 MIN','30 MIN'],accent:['Default','Cyan','Blue','Green','Amber','Violet'],density:['Compact','Comfortable','Large'],notifications:['All','Warnings and faults','Faults only'],timezone:['America/Chicago','America/New_York','UTC'],accessibility:['Standard','Large text','High contrast','Reduced motion']};
 if(sets[k]){choices(n,settings.flatMap(s=>s[1]).find(r=>r[3]===k)?.[0]||k,sets[k],'preference:'+k+':');return n;}
 if(k==='brightness'){modal(n,'Brightness','Enter a percentage (10–100).',[],true,'set-number:brightness');n.field=(n.preferences.brightness||'75%').replace('%','');n.numeric=true;return n;}
 if(k==='theme'){modal(n,'Theme','The preview follows the theme being edited. Use Original Studio to compare.',[['Custom themes','custom-themes'],['Close','close']]);return n;}
 if(k==='history'){modal(n,'Event history','Printer connected\nConfiguration loaded\nController ready',[['Close','close']]);return n;}
 if(k==='about'){modal(n,'About PrinterHMI','PrinterHMI 7.0.0\nESP32-P4 · LVGL 9.5\nTheme Maker offline tablet simulation',[['Close','close']]);return n;}
 if(k==='backup'){choices(n,'Configuration backup',['Backup plain','Backup encrypted','Restore plain','Restore encrypted'],'backup:');return n;}
 if(k==='ota'){modal(n,'OTA update','Firmware channel: Stable\nCurrent version: 7.0.0',[['Check for updates','ota-check'],['Close','close']]);return n;}
 if(k==='setup'){choices(n,'Setup center',['Wi-Fi','Printer','Camera'],'setup:');return n;}
 if(['reboot','restart','reset'].includes(k)){modal(n,k==='reboot'?'Reboot controller?':k==='restart'?'Restart Klipper?':'Factory reset?','This changes only the offline simulation.',[['Cancel','close'],['Confirm','maintenance:'+k]]);return n;}
 }
 if(action.startsWith('accessibility:')){const key=action.slice(14);if(['Large text','High contrast','Reduced transparency','Reduced motion'].includes(key)){n.accessibility[key]=!n.accessibility[key];n.preferences.accessibility=Object.values(n.accessibility).some(Boolean)?'Custom':'Standard';}return n;}
 if(action==='custom-themes'){modal(n,'Custom themes','Current editor theme is available in this preview.',[['Close','close']]);return n;}
 if(action==='ota-check'){n.message='Simulation complete: firmware is up to date.';return n;}
 if(action.startsWith('maintenance:'))return action==='maintenance:reset'?create():dispatch(n,'confirm-restart');
 if(action.startsWith('setup:'))return action==='setup:Camera'?dispatch(enter(n,'camera'),'camera-configure'):action==='setup:Wi-Fi'?enter(n,'network'):dispatch(n,'printers');
 if(action.startsWith('preference:')){const [,key,v]=action.split(':');if(settings.flatMap(s=>s[1]).some(r=>r[3]===key)){n.preferences[key]=v;n.modal=null;}return n;}
 if(action.startsWith('backup:')){const kind=action.slice(7);modal(n,kind,kind.includes('encrypted')?'Enter a sample password (demo password: studio).':'Only simulated data will be used.',[],kind.includes('encrypted'),'backup-complete:'+kind);return n;}
 if(action.startsWith('backup-complete:')){if(action.includes('Restore encrypted')&&n.field!=='studio'){n.message='Password mismatch. Check the password and try again.';return n;}modal(n,'Completed',action.includes('Restore')?'Configuration restored successfully (simulation).':'Backup completed successfully (simulation).',[['Close','close']]);n.field='';return n;}
 return n;
}
function dispatch(state,action,value){const next=transition(state,action,value);next.scroll=Math.max(0,Math.min(scrollMax(next),next.scroll));return next;}
function scrollMax(s){return s.page==='settings'?settings.reduce((h,x)=>h+48+x[1].length*69+26,0)-358:s.page==='files'?Math.max(0,(visibleFiles(s).length+(s.folder?0:1))*92-296):s.page==='telemetry'?250:s.page==='console'?Math.max(0,consoleRows(s).length*32-246):0;}
function consoleRows(s){return s.log.filter(([kind,message])=>(!s.hideTemps||kind!=='temperature')&&message.toLowerCase().includes(s.consoleQuery.toLowerCase())&&(s.consoleFilter==='All entries'||s.consoleFilter==='Errors + warnings'&&['error','warning'].includes(kind)||({Errors:'error',Warnings:'warning',Commands:'command',Responses:'response',System:'system'})[s.consoleFilter]===kind));}
const api={create,enter,dispatch,visibleFiles,consoleRows,scrollMax,settings,files,pages};if(typeof module!=='undefined')module.exports=api;else g.TabletModel=api;
})(typeof globalThis!=='undefined'?globalThis:this);
