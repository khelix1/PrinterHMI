/* Studio scene: firmware-owned 1024 × 600 geometry, with sample printer data.
 * Keep positions aligned with ui_dashboard_layout_profile.c and Studio page builders.
 * This is an SVG preview, not LVGL; font rasterization may differ on the panel. */
(function(g){
'use strict';
const T=typeof module!=='undefined'?require('./tablet-model'):g.TabletModel;
const dock=['dashboard','printer','files','camera','tools','console','drybox','settings'];
const titles={dashboard:'Dashboard',printer:'Printer',files:'Files',camera:'Camera',tools:'Tools',console:'Console',drybox:'Drybox',settings:'Settings',network:'Network',telemetry:'Telemetry'};
const escape=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&apos;'}[c]));
// LVGL's RGB565 display drops the low channel bits before expanding to RGB888.
const rgb565=hex=>{const n=parseInt(hex.slice(1),16);return '#'+[(n>>19)*255/31|0,((n>>10)&63)*255/63|0,((n>>3)&31)*255/31|0].map(n=>n.toString(16).padStart(2,'0')).join('');};
const iconPaths=[["M2 2 L10 2 L10 10 L2 10 L2 2", "M16 2 L24 2 L24 10 L16 10 L16 2", "M2 16 L10 16 L10 24 L2 24 L2 16", "M16 16 L24 16 L24 24 L16 24 L16 16"], ["M6 9 L6 2 L20 2 L20 9", "M6 19 L2 19 L2 9 L24 9 L24 19 L20 19", "M6 15 L20 15 L20 24 L6 24 L6 15"], ["M5 2 L16 2 L22 8 L22 24 L5 24 L5 2", "M16 2 L16 8 L22 8", "M9 14 L18 14", "M9 19 L18 19"], ["M2 7 L8 7 L10 3 L17 3 L19 7 L24 7 L24 23 L2 23 L2 7", "M10 10 L16 10 L19 13 L19 17 L16 20 L10 20 L7 17 L7 13 L10 10"], ["M3 23 L2 20 L13 9 L13 4 L17 1 L18 1 L16 6 L20 10 L25 7 L25 9 L22 13 L17 13 L6 24 L3 23"], ["M2 3 L24 3 L24 23 L2 23 L2 3", "M7 8 L11 12 L7 16", "M14 17 L20 17"], ["M3 8 L7 2 L20 2 L24 8 L24 24 L3 24 L3 8 L24 8", "M8 20 L10 17 L8 14 L10 11", "M14 20 L16 17 L14 14 L16 11"], ["M10 2 L16 2 L17 6 L21 6 L24 10 L21 13 L23 17 L20 21 L16 20 L14 24 L9 24 L8 20 L4 20 L2 16 L5 13 L3 9 L6 5 L10 6 L10 2", "M10 9 L16 9 L18 13 L16 17 L10 17 L8 13 L10 9"]];
function render(project,page='dashboard',options={}){
 const c=k=>rgb565(project.palette[k]||project.palette.text),out=[];
 const rect=(x,y,w,h,fill='background',radius=0,stroke=null)=>out.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${radius}" data-theme-fill="${fill}"${stroke?` data-theme-edge="${stroke}"`:""} fill="${c(fill)}"${stroke?` stroke="${c(stroke)}"`:''}/>`);
 const text=(value,x,y,size=18,color='text',anchor='start')=>out.push(`<text x="${x}" y="${y+size*.96}" data-theme-ink="${color}" font-size="${size}" font-weight="${size>=28?700:size>=20?600:400}" fill="${c(color)}" text-anchor="${anchor}">${escape(value)}</text>`);
 const line=(x,y,w,h=1)=>rect(x,y,w,h,'border');
 const button=(value,x,y,w=112,h=44,primary=false)=>{out.push(`<g data-theme-fill="${primary?'accent_2':'panel'}" data-theme-edge="${primary?'accent_2':'accent'}">`);rect(x,y,w,h,primary?'accent_2':'panel',Math.min(project.metrics.radius_button,h/2),primary?'accent_2':'accent');text(value,x+w/2,y+(h-20)/2-2,20,'text','middle');out.push('</g>');};
 const heading=value=>text(value,0,0,24);
 out.push(`<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="600" viewBox="0 0 1024 600" role="group" aria-label="${escape(titles[page]||'Studio')} Studio layout preview" font-family="PrinterHMI Inter, Inter, sans-serif" font-weight="400" style="font-optical-sizing:none;font-variation-settings:'opsz' 24"><defs><clipPath id="studio-body"><rect width="976" height="424"/></clipPath></defs>`);
 rect(0,0,1024,600);
 // Only accept the locally prepared PNG from the native artwork bridge.
 if(/^data:image\/png;base64,[A-Za-z0-9+/=]+$/.test(options.artwork||''))out.push(`<image href="${options.artwork}" width="1024" height="600"/>`);
 rect(0,0,1024,72,'background');text('PrinterHMI',24,22,24);text((options.state?.printer||'Sermoon D1')+'⌄',382,25,20,'text','middle');
 [0,1,2,3].forEach(i=>rect(670+i*8,42-i*4,5,6+i*4,'wifi_inactive'));
 text('2:58 PM',720,26,18,'text_dim');rect(852,10,140,52,'panel',Math.min(project.metrics.radius_button,26),'danger');text('⚠ E-STOP',922,24,20,'text','middle');
 const state=options.state||T.create();
 const a=(action,x,y,w,h,draw)=>{out.push(`<g data-tablet-action="${escape(action)}" role="button" tabindex="0"><rect x="${x}" y="${y}" width="${w}" height="${h}" fill="#000000" fill-opacity="0" pointer-events="all"/>`);draw?.();out.push('</g>');};
 const nativeButton=(value,x,y,w,h=48,action='',primary=false,disabled=false)=>{
  const fill=primary?'accent_2':null,edge=primary?'accent_2':'border_bright';
  out.push(`<g ${action&&!disabled?`data-tablet-action="${escape(action)}" role="button" tabindex="${disabled?-1:0}" aria-disabled="${disabled}"`:''} ${disabled?'opacity="0.4" aria-disabled="true"':''} ${fill?`data-theme-fill="${fill}"`:''} data-theme-edge="${edge}">`);
  out.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${primary?Math.min(project.metrics.radius_button,h/2):h/2}" pointer-events="all" fill="${fill?c(fill):'none'}" stroke="${c(edge)}"/>`);
  text(value,x+w/2,y+(h-18)/2-2,18,'text','middle');out.push('</g>');
 };
 const clip=(id,x,y,w,h)=>{out.push(`<defs><clipPath id="${id}"><rect x="${x}" y="${y}" width="${w}" height="${h}"/></clipPath></defs><g clip-path="url(#${id})">`);};
 const sampleModel=(x,y,w,h)=>{out.push(`<g transform="translate(${x} ${y})"><path d="M${w*.15} ${h*.8} L${w*.5} ${h*.95} ${w*.85} ${h*.8} ${w*.5} ${h*.65} Z" fill="${c('control')}" data-theme-fill="control"/><path d="M${w*.35} ${h*.32} L${w*.5} ${h*.22} ${w*.66} ${h*.32} ${w*.66} ${h*.7} ${w*.5} ${h*.8} ${w*.35} ${h*.7} Z M${w*.35} ${h*.32} L${w*.5} ${h*.42} ${w*.66} ${h*.32} M${w*.5} ${h*.42} V${h*.8}" fill="none" stroke="${c('accent_bright')}" stroke-width="2" data-theme-edge="accent_bright"/></g>`);};
 // Shell controls share state with every page; these are local actions only.
 a('printers',202,14,360,44);a('estop',852,10,140,52);
 out.push('<g transform="translate(24 80)" clip-path="url(#studio-body)">');
 const thermal=(x,y,name,current,target,key)=>{const draw=()=>{text(name,x,y,20,'text_dim');text(current,x,y+30,48,'success');text('Target '+target,x,y+90,18,'text_dim');};if(page==='printer')a('temperature:'+key,x-12,y,336,136,draw);else draw();};
 if(page==='dashboard'){
  text(state.print.toUpperCase(),0,0,20);text(state.job,0,32,24,'text_bright');
  a('fullscreen',0,62,316,276,()=>{text('PRINT',158,168,24,'text_dim','middle');text('THUMBNAIL',158,198,24,'text_dim','middle');});
  text(state.progress+'%',340,54,96);rect(340,176,360,12,'progress_track',6);rect(340,176,state.progress*3.6,12,'success',6);
  text('ETA 3:42 PM',340,194,18,'text_dim');text('Layer '+Math.round(state.progress*267/100)+' / 267',340,228,18,'text_dim');text('01:06 elapsed  •  REM 00:38',340,254,18);
  nativeButton(state.print==='paused'?'▶ Resume':'Ⅱ Pause',340,278,112,52,'pause',true,!['printing','paused'].includes(state.print));
  nativeButton('■ Stop',464,278,112,52,'stop',false,!['printing','paused'].includes(state.print));nativeButton('Object',588,278,112,52,'object',false,!['printing','paused'].includes(state.print)||state.excluded?.length>=2);
  line(736,0,1,330);thermal(756,4,'Nozzle','210°',state.nozzle+'°','nozzle');line(756,134,220);thermal(756,154,'Bed','60°',state.bed+'°','bed');
  text('LIVE',756,298,16,'success');text('PRESENT',856,298,16,'success');line(0,346,976);
  [['Speed','125 mm/s','tune'],['Flow','8.3 mm3/s','tune'],['Fan',state.fan+'%','fan'],['Drybox air','45.0 C','nav:drybox'],['Humidity','18.0 %RH','nav:drybox']].forEach(([v,b,action],i)=>{text(v,i*196,356,16,'text_dim');text(b,i*196,386,i>2?32:20);if(i)line(i*196-10,356,1,60);});
 }else if(page==='printer'){
  a('fullscreen',0,0,620,240,()=>{text('PRINT',310,86,24,'text_dim','middle');text('THUMBNAIL',310,116,24,'text_dim','middle');});
  text('X '+state.position[0].toFixed(1)+'    Y '+state.position[1].toFixed(1)+'    Z '+state.position[2].toFixed(2),0,252);nativeButton('Tune',524,246,96,44,'tune');
  line(0,302,620);[['Speed','125 mm/s'],['Flow','8.3 mm3/s'],['Layer','171 / 267'],['Filament','PRESENT']].forEach(([v,b],i)=>{const x=[0,144,288,474][i];text(v,x,310,16,'text_dim');text(b,x,332,16);});
  line(630,8,1,338);thermal(652,0,'Nozzle','210°',state.nozzle+'°','nozzle');thermal(652,142,'Bed','60°',state.bed+'°','bed');text('Fan',652,276,16,'text_dim');a('fan',652,294,94,40,()=>text(state.fan+'%',652,298,24));text('Elapsed',764,276,16,'text_dim');text('01:06',764,298,24);text('Remaining',876,276,16,'text_dim');text('00:38',876,298,24);
  [['Toolhead control',0,220,'toolhead'],['Home',232,128,'home'],['Pause',372,128,'pause'],['Resume',512,128,'pause'],['Object',652,128,'object'],['Stop',792,184,'stop']].forEach(([v,x,w,action])=>nativeButton(v,x,370,w,52,action,false,(v==='Pause'&&state.print!=='printing')||(v==='Resume'&&state.print!=='paused')||(['Object','Stop'].includes(v)&&!['printing','paused'].includes(state.print))||(v==='Object'&&state.excluded?.length>=2)));
 }else if(page==='files'){
  heading('Job library');[['Up',0,100,'up'],['Search',110,148,'files-search'],[['Name','Date','Size'][state.sort],268,144,'sort'],['Refresh',422,154,'refresh-files']].forEach(([v,x,w,action])=>nativeButton(v,x,48,w,44,action));
  text(state.message&&page==='files'?state.message:state.folder?'G-code files / '+state.folder:state.query?'Search: '+state.query:'G-code files',0,98,16,'text_dim');
  clip('file-list',0,128,576,296);out.push(`<g transform="translate(0 ${128-state.scroll})">`);
  if(!state.folder)a('folder',0,0,576,92,()=>{text('▱',24,22,32,'accent_bright');text('Calibration',94,20,20);text('Folder',94,50,16,'text_dim');line(0,91,576);});
  T.visibleFiles(state).forEach((f,i)=>{const y=(i+(state.folder?0:1))*92;a('file:'+f.index,0,y,576,92,()=>{if(state.selected===f.index)rect(0,y,576,91,'control',0);sampleModel(12,y+12,66,66);text(f.name,94,y+20,20);text(f.size+' MB · '+f.minutes+' min',94,y+50,16,'text_dim');line(0,y+91,576);});});out.push('</g></g>');line(600,0,1,424);
  if(state.selected===null){text('Select a file to inspect',624,154,20,'text_dim');text('its preview and metadata',624,180,20,'text_dim');}else{const f=T.files[state.selected];a('fullscreen',624,0,352,176,()=>sampleModel(668,0,264,176));text(f[0],624,188,24);text('Layer height: 0.20 mm',624,228,16,'text_dim');text('Filament: PLA · 12.5 m',624,254,16,'text_dim');text('Estimated time: '+f[2]+' min',624,280,16,'text_dim');text('Size: '+f[1]+' MB',624,306,16,'text_dim');nativeButton('Print',624,372,128,52,'file-print');nativeButton('Details',764,372,100,52,'file-details');nativeButton('Cancel',876,372,100,52,'file-cancel');}
 }else if(page==='drybox'){
  heading('Drybox');text(state.dryHeld?'Held':state.dryProgram==='No program'?'Ready':'Drying',976,0,24,'text_dim','end');text('Air',0,82,18,'text_dim');text('45.0 C',0,114,48);line(0,196,260);text('Center',0,222,18,'text_dim');text('44.5 C',0,254,48);
  out.push(`<path d="M368.5 311.5 A152 152 0 1 1 583.5 311.5" fill="none" data-theme-edge="border" stroke="${c('border')}" stroke-width="16"/><path d="M368.5 311.5 A152 152 0 0 1 324.5 194.5" fill="none" data-theme-edge="success" stroke="${c('success')}" stroke-width="16"/>`);
  text('18',476,118,96,'text','middle');text('% RH',494,232,24,'text_dim','middle');text('Dry',486,282,16,'success','middle');
  (()=>{text('Target temperature',716,76,18,'text_dim');text(state.dryTarget+' C',716,110,48);})();line(716,196,260);text('Heater',716,222,18,'text_dim');text('Fan',856,222,18,'text_dim');text(state.dryTarget&&!state.dryHeld?'ON':'OFF',716,260,24);text(state.dryTarget&&!state.dryHeld?'65%':'0%',856,260,24);text(state.dryProgram+(state.dryHeld?' · Held':''),0,330,18,'text_dim');line(0,358,976);
  ['PLA','PETG','Hold','Resume','Stop'].forEach((v,i)=>nativeButton(v,i*196,372,192,52,'dry:'+v));
 }else if(page==='network'){
  heading('Network');nativeButton('Scan networks',568,4,196,48,'scan');nativeButton('Manage printers',776,4,200,48,'printers');text('Connected',16,84,20,'success');text('Wi-Fi and Moonraker are online',212,86,18,'text_dim');line(0,123,976);
  text('Wi-Fi',0,140,20);[['Network',state.wifi],['Address','192.168.1.42'],['Signal','Excellent']].forEach(([v,b],i)=>{text(v,0,192+i*76,16,'text_dim');text(b,0,218+i*76,20);});line(282,140,1,284);
  text('Moonraker',294,140,20);[['Host','192.168.1.100'],['Port','7125'],['Connection','Connected'],['Last HTTP','200 OK']].forEach(([v,b],i)=>{text(v,294,188+i*60,16,'text_dim');text(b,294,214+i*60,20);});line(608,140,1,284);
  text('Available networks',620,140,20);text(state.message||'Select a network to connect',620,176,16,'text_dim');['Studio Network','Workshop','Guest'].forEach((v,i)=>a('wifi:'+v,620,236+i*56,356,56,()=>{text(v,620,244+i*56,20);line(620,282+i*56,356);}));
 }else if(page==='settings'){
  heading('Settings');clip('settings-content',0,66,976,358);out.push(`<g transform="translate(0 ${66-state.scroll})">`);let sy=0;
  for(const [title,rows]of T.settings){text(title,0,sy+6,24,'text_dim');rows.forEach(([title,desc,value,key,action],i)=>{const y=sy+48+i*69;const row=()=>{text(title,0,y+6,20);if(desc)text(desc,0,y+38,16,'text_dim');if(action)nativeButton(value,700,y+10,276,48,'setting:'+key);else if(key==='brightness'){rect(586,y+32,286,10,'progress_track',5);rect(586,y+32,286*parseFloat(state.preferences.brightness||75)/100,10,'accent',5);out.push(`<circle cx="${586+286*parseFloat(state.preferences.brightness||75)/100}" cy="${y+37}" r="11" fill="${c('text')}" data-theme-fill="text"/>`);text(state.preferences.brightness||value,976,y+22,18,'text','end');}else{text(state.preferences[key]||value,930,y+22,18,'text','end');if(key)text('›',944,y+24,18,'accent_bright');}line(0,y+67,976);};if(key&&key!=='brightness'&&!action)a('setting:'+key,0,y,976,68,row);else row();});sy+=48+rows.length*69+26;}out.push('</g></g>');
 }else if(page==='telemetry'){
  heading('Telemetry');nativeButton('Devices',152,0,128,48,'tool:Devices');text(state.held?'Graph held · values live':'Live',976,14,16,'text_dim','end');
  [['Thermal',0,96,'telemetry:Thermal'],['Motion',104,96,'telemetry:Motion'],['Drybox',208,96,'telemetry:Drybox'],[state.range+' min',312,120,'range'],['Nozzle',440,170,'hotend'],[state.targets?'Targets on':'Targets off',618,160,'targets'],[state.held?'Resume live':'Hold graph',786,190,'hold']].forEach(([v,x,w,action])=>{if(action==='hotend'&&state.telemetry!=='Thermal')return;nativeButton(v,x,56,w,48,action,state.telemetry===v,state.held&&action!=='hold');});
  clip('charts',0,112,976,285);out.push(`<g transform="translate(0 ${112-state.scroll})">`);
  const channels=state.telemetry==='Thermal'?[['Nozzle','210.0 C'],['Bed','60.0 C'],['Part fan','65.0 %']]:state.telemetry==='Motion'?[['Velocity','125.0 mm/s'],['Volumetric flow','8.3 mm3/s'],['Speed override',state.speed+'.0 %'],['Flow override',state.flow+'.0 %']]:[['Drybox center','44.5 C'],['Drybox air','45.0 C'],['Humidity','18.0 %RH'],['Drybox fan','65.0 %']];
  channels.forEach(([v,value],i)=>{const x=(i%2)*494,y=Math.floor(i/2)*274,ink=['accent_cyan','warning','telemetry_chamber','telemetry_humidity'][i];text(v,x+12,y+12,18,ink);text(value,x+470,y+12,20,'text_bright','end');text(state.targets&&i<2?'Target enabled | Error +0.0':'Live reading',x+12,y+44,16,'text_dim');rect(x+12,y+72,458,96,'telemetry_chart_bg',12);for(let j=1;j<4;j++)rect(x+12,y+72+j*24,458,1,'telemetry_grid');let d='';for(let j=0;j<=24;j++)d+=(j?' L':'M')+(x+12+j*458/24)+' '+(y+114+Math.sin(j*.7*state.range/2+i)*12+Math.cos(j*.3)*6);out.push(`<path d="${d}" fill="none" stroke="${c(ink)}" stroke-width="2" data-theme-edge="${ink}"/>`);if(state.targets)rect(x+12,y+118,458,1,'text_dim');text('Auto scale · '+state.range+' minute window',x+12,y+176,16,'text_dim');text('Min / Avg / Max · sample history',x+12,y+203,16);text('-'+state.range+' min',x+12,y+232,16,'text_dim');text('NOW',x+470,y+232,16,'text_dim','end');line(x,y+259,482);});out.push('</g></g>');text('Local sample history · '+(state.held?'held':'live')+' · scroll for more channels',0,405,16,'text_dim');
 }else if(page==='camera'){
  rect(0,0,976,424,'background');if(state.cameraConfigured){out.push(`<g transform="translate(488 183) rotate(${state.cameraRotation||0}) scale(${state.cameraMirrorH?-1:1} ${state.cameraMirrorV?-1:1}) translate(-488 -183)">`);sampleModel(250,38,476,290);out.push('</g>');}text(state.cameraConfigured?'Simulated camera '+state.camera+' · '+state.cameraFit:'No camera configured',22,22,18,'text_dim');
  nativeButton('Configure',100,356,196,48,'camera-configure');nativeButton('View',308,356,120,48,'camera-view');nativeButton('Camera '+state.camera,448,356,180,48,'camera-next');nativeButton('Fullscreen',718,356,210,48,'fullscreen');
 }else if(page==='tools'){
  heading('Tools');[['Calibration','Tune and validate your printer.','↻','success'],['Bed mesh','Probe and visualize the bed surface.','▦','accent_bright'],['Devices','Inspect devices and live readings.','ϟ','warning'],['Macros','Run available printer macros.','▶','success']].forEach(([v,body,icon,ink],i)=>{const x=i%2*496,y=56+Math.floor(i/2)*184;a('tool:'+v,x,y,480,168,()=>{text(icon,x+20,y+16,32,ink);text(v,x+72,y+16,24,'text_bright');text(body,x+72,y+56,18);line(x,y+167,480);});});line(488,64,1,352);
 }else if(page==='console'){
  heading('Console');text(T.consoleRows(state).length+' shown',976,7,16,'text_dim','end');text(state.printer+' | Connected',0,38,16,'text_dim');
  [[state.consoleFilter,0,260,'console-filter'],[state.hideTemps?'Temps hidden':'Temps shown',268,160,'temps'],['Search',436,104,'console-search'],['Reset filters',548,144,'reset-filters']].forEach(([v,x,w,action])=>nativeButton(v,x,65,w,48,action));
  line(0,121,976);line(0,368,976);clip('console-log',0,122,976,246);T.consoleRows(state).forEach(([kind,value],i)=>text(value,10,132+i*32-state.scroll,18,kind==='error'?'danger':kind==='command'?'accent_bright':'text'));out.push('</g>');
  nativeButton('Command',0,376,136,48,'command',true);nativeButton(state.follow?'Follow on':'Follow off',144,376,136,48,'follow');nativeButton('Clear',288,376,96,48,'clear-console');
 }
 out.push('</g>');
 dock.forEach((name,i)=>{const x=30+i*120,active=page===name;if(active)rect(x,523,118,54,'accent_bright',Math.min(project.metrics.radius_button,27));out.push(`<g transform="translate(${x+46} 524)" fill="none" data-theme-ink="${active?'background':'text_dim'}" stroke="${c(active?'background':'text_dim')}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">${iconPaths[i].map(d=>`<path d="${d}"/>`).join('')}</g>`);text(titles[name],x+59,555,14,active?'background':'text_dim','middle');});
 renderOverlay(out,project,state,{rect,text,line,button,nativeButton,a,sampleModel,c});
 out.push('</svg>');return out.join('');
}
// Popup sizes copied from native builders at the 1024 × 600 panel resolution.
const popupSizes={
 'Stop print?':[620,320],'Cancel object?':[560,320],'Cancel object':[800,520],
 'Start print?':[640,340],'Home all axes?':[620,320],'Stop request sent':[680,400],
 'Nozzle temperature':[660,460],'Bed temperature':[660,460],'Part cooling fan':[660,460],
 'Custom nozzle temperature':[600,500],'Custom bed temperature':[600,500],
 'Print tuning':[680,280],'Camera view':[680,460],'Choose printer':[900,540],
 'Time zone':[760,500],'Theme':[920,520],'Custom themes':[820,520],
 'Accent color':[720,568],'Display density':[640,346],'Accessibility':[720,568],
 'Reboot controller?':[680,330],'Restart Klipper?':[680,330],'Factory reset?':[680,330],
 'Bed mesh profiles':[760,520],'Configuration backup':[880,540],
 'Toolhead control':[960,568],'Send command':[820,520],'Search console':[820,520],
 'Search files':[760,540]
};
function dialogGeometry(state){
 const m=state.modal||{},size=popupSizes[m.title]||(m.input?[760,540]:[760,480]);
 const [w,h]=size,x=(1024-w)/2,y=(600-h)/2,footerY=y+h-64;
 return {x,y,w,h,footerY,...(m.input?{input:{x:x+16,y:y+86,w:w-32,h:56},keyboardY:footerY-208}:{})};
}
function renderOverlay(out,project,state,draw){const {rect,text,line,nativeButton,a,sampleModel,c}=draw;
 if(state.fullscreen){out.push('<g data-tablet-modal="true">');rect(0,0,1024,600,'background');a('close',0,0,1024,600,()=>sampleModel(212,50,600,460));if(state.page==='camera')nativeButton('Exit fullscreen',728,536,280,48,'close');out.push('</g>');return;}
 if(!state.modal)return;
 const box=dialogGeometry(state),m=state.modal;
 out.push('<g data-tablet-modal="true"><rect width="1024" height="600" fill="#000000" opacity="0.65"/>');rect(box.x,box.y,box.w,box.h,'popup',22,'border_popup');text(m.title,box.x+16,box.y+16,24);line(box.x+16,box.y+64,box.w-32);
 if(m.title==='Toolhead control'){
  const toolButton=(v,x,y,w,action,selected=false)=>a(action,x,y,w,44,()=>draw.button(v,x,y,w,44,selected));
  text('X '+state.position[0].toFixed(1)+' | Y '+state.position[1].toFixed(1)+' | Z '+state.position[2].toFixed(3)+' mm',48,94,24,'text_bright');text(state.message||'Homed: XYZ',952,98,16,'accent_cyan','end');
  ['Move','Filament','Z offset'].forEach((v,i)=>toolButton(v,48+i*310,134,300,'toolhead-tab:'+i,state.toolheadTab===i));
  if(state.toolheadTab===0){text('XY jog',48,194,18,'text_dim');[['↑ Y+',1,0,'move:1:1'],['← X−',0,1,'move:0:-1'],['Home',1,1,'confirm-home'],['X+ →',2,1,'move:0:1'],['↓ Y−',1,2,'move:1:-1']].forEach(([v,x,y,action])=>toolButton(v,48+x*148,224+y*52,140,action));
   text('XY step (mm)',48,388,18,'text_dim');[1,10,50].forEach((v,i)=>toolButton(v+' mm',48+i*148,418,140,'xy-step:'+v,state.step===v));
   text('Z jog',528,194,18,'text_dim');toolButton('↑ Z+',528,224,208,'move:2:1');toolButton('↓ Z−',748,224,208,'move:2:-1');text('Z step (mm)',528,286,18,'text_dim');[0.01,0.05,0.1,0.5,1,5,10,25].forEach((v,i)=>toolButton(v.toFixed(2),528+i%4*110,316+Math.floor(i/4)*52,100,'z-step:'+v,state.zStep===v));
  }else if(state.toolheadTab===1){text('Extrusion length (mm)',48,198,18,'text_dim');[1,5,10,25,50].forEach((v,i)=>toolButton(String(v),48+i*184,230,176,'extrusion-step:'+v,state.extrusionStep===v));text('Extrusion direction',48,300,18,'text_dim');toolButton('Extrude',48,334,448,'extrude');toolButton('Retract',508,334,448,'retract');
  }else{text('Runtime Z offset (mm)',48,198,18,'text_dim');text((state.zOffset>=0?'+':'')+state.zOffset.toFixed(3)+' mm',952,198,24,'text_bright','end');[-0.005,-0.01,-0.025,-0.05,-0.1,0.005,0.01,0.025,0.05,0.1].forEach((v,i)=>toolButton((v>0?'+':'')+v,48+i%5*184,248+Math.floor(i/5)*56,176,'offset:'+v));toolButton('Reset offset',48,376,908,'offset:reset');}
  toolButton('Close',780,520,176,'close');out.push('</g>');return;
 }
 const footer=(items)=>{line(box.x+16,box.footerY-10,box.w-32);const gap=8,w=(box.w-32-(items.length-1)*gap)/items.length;items.forEach(([label,action,primary],i)=>nativeButton(label,box.x+16+i*(w+gap),box.footerY,w,48,action,!!primary));};
 const option=(label,action,y,selected=false,right='',height=46)=>a(action,box.x+16,y,box.w-32,height,()=>{if(selected)rect(box.x+16,y,box.w-32,height,'control',8);text(label,box.x+30,y+11,18);text(right||(selected?'✓':''),box.x+box.w-32,y+11,18,selected?'accent_bright':'text_dim','end');line(box.x+16,y+height-1,box.w-32);});
 if(m.control){
  const key=m.control,fan=key==='fan',values=fan?[0,25,50,75,100]:key==='bed'?[50,60,70,80,100,0]:[180,200,215,230,250,0];
  text(fan?'Current speed':'Current',box.x+16,box.y+86,16,'text_dim');text((fan?state.fan:key==='bed'?60:210)+(fan?'%':' C'),box.x+16,box.y+116,32);
  if(!fan)a('custom:'+key,box.x+box.w/2,box.y+80,box.w/2-16,98,()=>{text('Target - tap to edit',box.x+box.w/2,box.y+86,16,'text_dim');text(state[key]?state[key]+' C':'Off',box.x+box.w/2,box.y+116,32,'accent_cyan');});
  line(box.x+16,box.y+180,box.w-32);text('Choose a preset',box.x+16,box.y+194,16,'text_dim');
  const w=(box.w-48)/3;values.forEach((v,i)=>nativeButton(v===0?'Off':v+(fan?'%':' C'),box.x+16+i%3*(w+8),box.y+228+Math.floor(i/3)*60,w,48,'preset:'+key+':'+v,state[key]===v));footer([['Back','close']]);out.push('</g>');return;
 }
 if(m.title==='Print tuning'){
  ['speed','flow'].forEach((key,i)=>{out.push(`<g data-tuning="${key}">`);const x=box.x+24+i*332;text(i?'Flow':'Speed',x,box.y+80,18,'text_dim');text(state[key]+'%',x,box.y+110,32);rect(x,box.y+164,280,8,'progress_track',4);rect(x,box.y+164,280*(state[key]-50)/100,8,i?'success':'accent',4);out.push(`<circle cx="${x+280*(state[key]-50)/100}" cy="${box.y+168}" r="10" fill="${c('text')}"/>`);out.push('</g>');});nativeButton('Close',box.x+496,box.y+214,160,48,'close');out.push('</g>');return;
 }
 if(m.title==='Camera view'){
  text(m.body,box.x+16,box.y+82,20,'text_bright');text('Rotation',box.x+16,box.y+124,16,'text_dim');
  [0,90,180,270].forEach((v,i)=>nativeButton(v+' deg',box.x+16+i*164,box.y+152,156,48,'camera-rotation:'+v,state.cameraRotation===v));
  text('Mirror',box.x+16,box.y+222,16,'text_dim');nativeButton('Horizontal',box.x+16,box.y+250,316,48,'camera-mirror-h',state.cameraMirrorH);nativeButton('Vertical',box.x+348,box.y+250,316,48,'camera-mirror-v',state.cameraMirrorV);footer([['Close','close'],['Reset view','camera-reset']]);out.push('</g>');return;
 }
 if(m.title==='Accessibility'){
  text('Tap an option to toggle it.',box.x+16,box.y+82,16,'text_dim');
  ['Large text','High contrast','Reduced transparency','Reduced motion'].forEach((v,i)=>option(v,'accessibility:'+v,box.y+122+i*76,!!state.accessibility[v],state.accessibility[v]?'ON':'OFF'));footer([['Close','close']]);out.push('</g>');return;
 }
 const body=m.title==='Toolhead control'?'X '+state.position[0].toFixed(1)+'    Y '+state.position[1].toFixed(1)+'    Z '+state.position[2].toFixed(2):m.body;
 String(body).split('\n').flatMap(line=>line.match(/.{1,82}(?:\s|$)|\S{1,82}/g)||['']).slice(0,m.input?2:6).forEach((v,i)=>text(v.trim(),box.x+16,box.y+(m.input?148:82)+i*26,18,'text_dim'));
 if(m.input){
  rect(box.input.x,box.input.y,box.input.w,box.input.h,'background_deep',14,'accent_bright');
  // A real HTML input is placed over this well in the interactive desktop view.
  text((/encrypted|Connect to/.test(m.title)?'•'.repeat(state.field.length):state.field).slice(-60),box.input.x+12,box.input.y+12,18);
  const key=(label,value,x,y,w,action='key')=>{out.push(`<g data-tablet-action="${escape(action)}" data-tablet-value="${escape(value)}" role="button" tabindex="0">`);rect(x,y,w,42,'control',10);text(label,x+w/2,y+11,18,'text','middle');out.push('</g>');};
  if(state.numeric){
   const rows=[['1','2','3','⌨'],['4','5','6','✓'],['7','8','9','⌫'],['+/-','0','.','←','→']];
   rows.forEach((row,i)=>{const w=(box.w-32-(row.length-1)*6)/row.length;row.forEach((v,j)=>key(v,v,box.x+16+j*(w+6),box.keyboardY+i*48,w,v==='✓'?m.submit:v==='⌨'?(m.returnControl?'back-control':'close'):'key'));});
  }else{
   ['qwertyuiop','asdfghjkl','⇧zxcvbnm⌫'].forEach((row,i)=>{const keys=Array.from(row),w=(box.w-32-(keys.length-1)*6)/keys.length;keys.forEach((v,j)=>key(state.shift?v.toUpperCase():v,v,box.x+16+j*(w+6),box.keyboardY+i*48,w));});
   key('123','123',box.x+16,box.keyboardY+144,112);key('Space',' ',box.x+136,box.keyboardY+144,box.w-272);key('⌫','⌫',box.x+box.w-128,box.keyboardY+144,112);
  }
  if(m.title==='Send command'){
   [['Close','close'],['Previous','command-prev'],['Next','command-next'],['Send',m.submit]].forEach(([v,action],i)=>nativeButton(v,box.x+16+i*(box.w-24)/4,box.footerY,(box.w-56)/4,48,action,i===3));
  }else{nativeButton(m.returnControl?'Back':'Cancel',box.x+16,box.footerY,180,48,m.returnControl?'back-control':'close');nativeButton(m.returnControl?'Set':m.submit.includes('search')?'Search':'Apply',box.x+box.w-228,box.footerY,212,48,m.submit,true);}

 }else{
  const confirmation=m.choices.length===2&&m.choices.some(x=>/^(confirm-|maintenance:|exclude:|macro-run)/.test(x[1]));
  if(confirmation){footer(m.choices.map(([label,action],i)=>[label,action,i===1]));}
  else{
   const items=m.choices.filter(([label,action])=>!(action==='close'&&['Close','Back','Cancel'].includes(label)));
   const start=box.y+(m.body?148:82),step=Math.min(56,Math.floor((box.footerY-26-start)/Math.max(items.length,1)));
   items.forEach(([label,action],i)=>{const preference=action.startsWith('preference:')?action.split(':'):null;const selected=preference&&state.preferences[preference[1]]===preference.slice(2).join(':');option(label,action,start+i*step,selected,'',Math.min(46,step-4));});
   footer(m.submit?[['Cancel','close'],['Continue',m.submit,true]]:[['Close','close']]);
  }
 }
 if(state.message)text(state.message.slice(0,78),box.x+16,m.input?box.y+182:box.footerY-24,16,'warning');
 out.push('</g>');
}
// Specimens mirror ui_theme_studio.c and the native Settings/form helpers.
function renderComponents(project){
 const c=k=>rgb565(project.palette[k]||project.palette.text),out=[];
 const r=(x,y,w,h,fill,rx=0,edge=null)=>out.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${rx}" data-theme-fill="${fill}"${edge?` data-theme-edge="${edge}"`:""} fill="${c(fill)}"${edge?` stroke="${c(edge)}"`:''}/>`);
 const t=(v,x,y,size=18,ink='text',anchor='start')=>out.push(`<text x="${x}" y="${y+size*.96}" data-theme-ink="${ink}" font-size="${size}" font-weight="${size>=28?700:size>=20?600:400}" fill="${c(ink)}" text-anchor="${anchor}">${escape(v)}</text>`);
 const rule=(x,y,w)=>r(x,y,w,1,'border');
 const b=(v,x,y,w,fill='panel',edge='accent',disabled=false)=>{out.push(`<g data-theme-fill="${fill}" data-theme-edge="${edge}">`);if(disabled)out.push('<g opacity="0.4">');r(x,y,w,48,fill,Math.min(project.metrics.radius_button,24),edge);t(v,x+w/2,y+13,20,'text','middle');if(disabled)out.push('</g>');out.push('</g>');};
 out.push(`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1024 940" role="img" aria-label="Studio components: buttons, inputs, toggles, progress, sliders, status banners, cards and confirmation dialog" font-family="PrinterHMI Inter, Inter, sans-serif" style="font-optical-sizing:none;font-variation-settings:'opsz' 24">`);
 r(0,0,1024,940,'background');t('Studio components',32,22,28);t('Native control recipes · sample states',32,62,18,'text_dim');
 t('Buttons',32,110,24);t('Inputs',528,110,24);rule(32,147,464);rule(528,147,464);
 b('Primary',32,166,144,'accent_2','accent_2');b('Outlined',192,166,144);b('Stop',352,166,144,'panel','danger');
 b('Cancel',32,230,144,'panel','border_control');b('Success',192,230,144,'control','success');b('Disabled',352,230,144,'accent_2','accent_2',true);
 r(528,166,464,48,'background_deep',14,'border');t('Theme name',544,180,18);
 r(528,230,464,48,'background_deep',14,'accent_bright');t('Focused input',544,244,18);r(674,241,1,25,'text');
 t('On / off settings',32,310,24);t('Progress & adjustment',528,310,24);rule(32,347,464);rule(528,347,464);
 t('Large text',32,366,20);t('ON',496,371,18,'text','end');t('Increase text size',32,398,16,'text_dim');rule(32,429,464);
 t('Reduced motion',32,446,20);t('OFF',496,451,18,'text','end');t('Keep transitions enabled',32,478,16,'text_dim');rule(32,509,464);
 t('Restoring backup',528,366,20);t('64%',992,371,18,'text_dim','end');r(528,407,464,12,'progress_track',6);r(528,407,297,12,'success',6);
 t('Brightness',528,446,20);t('75%',992,451,18,'text_dim','end');r(528,487,464,12,'progress_track',6);r(528,487,348,12,'accent',6);out.push(`<circle cx="876" cy="493" r="12" data-theme-fill="text" fill="${c('text')}"/>`);
 t('Surfaces & feedback',32,546,24);t('Confirmation dialog',528,546,24);rule(32,583,464);rule(528,583,464);
 r(32,602,220,112,'card',project.metrics.radius_card,'border');t('Nozzle',48,616,18,'text_dim');t('210°',48,646,48,'success');
 r(268,602,228,112,'panel',project.metrics.radius_panel,'border');t('Printer connected',284,618,20);t('Moonraker online',284,656,18,'text_dim');
 [['Ready to print','success'],['Waiting for heat','warning'],['Connection interrupted','danger']].forEach(([v,color],i)=>{r(32,736+i*58,464,48,'panel',18,color);t(v,48,749+i*58,18);});
 r(528,602,464,296,'popup',22,'border');t('Apply custom theme?',548,622,24);rule(548,664,424);
 t('Use this design on the panel.',548,684,18);t('Your current settings will be retained.',548,718,18,'text_dim');rule(548,810,424);
 b('Cancel',548,830,196,'panel','border_control');b('Apply',756,830,216,'accent_2','accent_2');
 out.push('</svg>');return out.join('');
}
// Resolve only known palette tokens. Editor hit testing never accepts CSS or code.
function colorRoles(target,palette){
 const result=[],seen=new Set();
 for(let node=target;node&&node.tagName?.toLowerCase()!=='svg';node=node.parentElement){
  for(const [role,title]of [['ink','Text / icon'],['fill','Fill'],['edge','Border / line']]){
   const key=node.getAttribute?.('data-theme-'+role);
   if(key&&Object.hasOwn(palette,key)&&!seen.has(role)){seen.add(role);result.push({role,title,key});}
  }
 }
 return result;
}
const api={popupSizes,dialogGeometry,render,renderComponents,colorRoles,dock,titles,rgb565};if(typeof module!=='undefined')module.exports=api;else g.StudioScene=api;
})(typeof globalThis!=='undefined'?globalThis:this);
