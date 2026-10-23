/* Application event/state regression fixture. No browser or rendering engine. */
'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const M=require('./theme-model.js');
class Element{
 constructor(tag='div'){this.tagName=tag.toUpperCase();this.children=[];this.dataset={};this.style={setProperty(){}};this.classList={toggle(){}};this.clientWidth=1100;this.clientHeight=720;this.attrs={};this.textContent='';}
 append(...nodes){for(const n of nodes){this.children.push(n);if(n&&typeof n==='object')n.parentElement=this;}}
 replaceChildren(...nodes){this.children=[];this.append(...nodes);}
 getAttribute(k){return this.attrs[k]??null;}
 querySelectorAll(){return [];}
 setAttribute(k,v){this.attrs[k]=v;}removeAttribute(k){delete this.attrs[k];}
 addEventListener(k,fn){this['on'+k]=fn;}removeEventListener(){}
 showModal(){this.open=true;}close(){this.open=false;}
 focus(){}remove(){}setPointerCapture(){}
 getBoundingClientRect(){return {width:1056};}
 querySelector(selector){if(selector==='.p-body')return new Element();return this.walk().find(e=>selector.startsWith('.')?e.className===selector.slice(1):e.tagName===selector.toUpperCase())||null;}
 walk(){return this.children.filter(e=>e instanceof Element).flatMap(e=>[e,...e.walk()]);}
 click(){if(!this.disabled)this.onclick?.();}
}
const html=fs.readFileSync(__dirname+'/index.html','utf8'),els={};
for(const m of html.matchAll(/\bid="([^"]+)"/g)){assert(!els[m[1]],'duplicate id');els[m[1]]=new Element();}
const tabs=['design','layout','project'].map(v=>{const el=new Element('button');el.dataset.tab=v;return el;});
const views=['device','components','json'].map(v=>{const el=new Element('button');el.dataset.view=v;return el;});
els['preview-wrap'].append(els.device);
const storage=new Map(),outbound=[],documentHandlers={},intervals=[];
const document={getElementById:id=>{assert(els[id],'Missing element '+id);return els[id];},createElement:tag=>new Element(tag),createTextNode:text=>({textContent:text}),documentElement:new Element(),body:new Element(),activeElement:new Element(),addEventListener(k,fn){documentHandlers[k]=fn;},querySelector:()=>null,querySelectorAll:s=>s==='[data-tab]'?tabs:views};
const context={TabletModel:require('./tablet-model'),TextEncoder,ThemeProject:require('./project-model'),StudioScene:require('./studio-scene.js'),ThemeModel:M,document,console,confirm:()=>true,localStorage:{getItem:k=>storage.get(k)||null,setItem:(k,v)=>storage.set(k,v)},ResizeObserver:class{observe(){}},setTimeout(){},setInterval(fn){intervals.push(fn);return intervals.length;},clearInterval(){},clearTimeout(){},webkit:{messageHandlers:{themeMaker:{postMessage:v=>outbound.push(JSON.parse(v))}}}};
context.window=context;vm.createContext(context);vm.runInContext(fs.readFileSync(__dirname+'/app.js','utf8'),context);
const project=()=>JSON.parse(els.package.textContent);
const before=project();
els.workspace.click();assert(els.welcome.open,'upper-left control opens chooser');
els['cancel-welcome'].click();assert(!els.welcome.open);assert.deepEqual(project(),before);
els.new.click();els['blank-theme'].click();assert(!els.welcome.open);
assert.equal(project().name,'Untitled theme');assert(Object.values(project().layouts.dashboard).every(r=>!r.visible));
assert.equal(tabs[1].attrs['aria-selected'],'true');
const add=els.editor.walk().find(e=>e.tagName==='BUTTON'&&e.textContent==='＋ Active print');assert(add);add.click();
assert(project().layouts.dashboard.active_print.visible);
let region=els.device.walk().find(e=>e.dataset.region==='active_print');assert(region);
region.onkeydown({key:'ArrowRight',preventDefault(){},shiftKey:true});assert.equal(project().layouts.dashboard.active_print.x,26);
region=els.device.walk().find(e=>e.dataset.region==='active_print');
region.onpointerdown({preventDefault(){},clientX:100,clientY:100,pointerId:1,target:region.children.at(-1)});
region.onpointermove({clientX:130,clientY:120});region.onpointerup();
assert.equal(project().layouts.dashboard.active_print.width,428);assert.equal(project().layouts.dashboard.active_print.height,324);
region=els.device.walk().find(e=>e.dataset.region==='active_print');region.onkeydown({key:'Delete',preventDefault(){}});assert(!project().layouts.dashboard.active_print.visible);
els.undo.click();assert(project().layouts.dashboard.active_print.visible);
els.export.click();assert.equal(outbound.length,1);assert.deepEqual(M.parse(outbound[0].text),project());
assert(storage.size===1);console.log('PASS: upper-left chooser, cancel, blank creation, add/move/resize/delete block, undo, draft save and desktop export event flow (DOM fixture; no visual rendering)');
function control(label){const result=els.editor.walk().find(e=>e.attrs['aria-label']===label);assert(result,'Missing control '+label);return result;}
function drag(dx,dy,alt=false,cancel=false){const el=els.device.walk().find(e=>e.dataset.region==='active_print');el.onpointerdown({preventDefault(){},clientX:100,clientY:100,pointerId:1,target:el});el.onpointermove({clientX:100+dx,clientY:100+dy,altKey:alt});(cancel?el.onpointercancel:el.onpointerup)();}
control('Snap to grid').checked=true;control('Snap to grid').onchange();
drag(5,5);assert.equal(project().layouts.dashboard.active_print.x,32);assert.equal(project().layouts.dashboard.active_print.y,88);
drag(3,3,true);assert.equal(project().layouts.dashboard.active_print.x,35);assert.equal(project().layouts.dashboard.active_print.y,91);
const prior=project();drag(20,20,false,true);assert.deepEqual(project(),prior,'pointer cancellation restores composition');
control('Align block right').click();assert.equal(project().layouts.dashboard.active_print.x,426);
els.undo.click();assert.deepEqual(project(),prior);
control('Align block fill width').click();assert.equal(project().layouts.dashboard.active_print.width,822);assert.equal(project().layouts.dashboard.active_print.x,16);
assert(!Object.hasOwn(project(),'editor'),'editor preferences must not leak into firmware exports');
const stored=JSON.parse([...storage.values()][0]);assert(stored.editor.snap);assert.equal(stored.editor.size,8);
console.log('PASS: snap and Alt bypass event flow, cancelled drag rollback, alignment/undo, fill and separate persisted editor preferences');
tabs[0].click();context.ThemeMakerArtworkSelected({asset:'bg-'+'a'.repeat(64)+'.phmibg'});assert.equal(project().schema_version,3);assert(project().background);
let previewRequest=outbound.at(-1);assert.equal(previewRequest.action,'preview-artwork');
context.ThemeMakerArtworkPreview({key:previewRequest.key,data_url:'data:image/png;base64,AAAA'});
assert.equal(els.export.textContent,'Export theme bundle ↗');
els.export.click();assert.equal(JSON.parse(outbound.at(-1).text).schema_version,3);
const removeArt=els.editor.walk().find(e=>e.textContent==='Remove background image');assert(removeArt);removeArt.click();assert.equal(project().schema_version,1);assert(!project().background);
els.undo.click();assert.equal(project().schema_version,3);assert(project().background);
const invalidProject=M.copy(project());invalidProject.background.asset='../outside.phmibg';const safePrior=project();context.ThemeMakerProjectLoaded({project:invalidProject});assert.deepEqual(project(),safePrior);
console.log('PASS: native artwork callback, bounded preview request, ZIP export action, removal/downgrade, undo and atomic invalid-import rejection');

// The Studio preset must retain every firmware color, not the generic preset remap.
els.workspace.click();const studio=els['starter-grid'].children[0];studio.click();
assert.equal(project().base_theme,'studio');assert.deepEqual(project().palette,M.create('studio').palette);
assert.deepEqual(project().metrics,M.create('studio').metrics);
const nativeDock=els.device.walk().find(e=>e.className==='studio-dock-hit');assert(nativeDock);assert.equal(nativeDock.children.length,8);
nativeDock.children[3].click();assert.equal(els['preview-label'].textContent,'CAMERA');assert(!els.export.disabled);
els['pages'].children.find(e=>e.textContent==='Dashboard').click();assert.equal(els['preview-label'].textContent,'DASHBOARD');
console.log('PASS: original Studio preset palette/metrics and eight-item Studio navigation, including Camera and return to Dashboard');

views[1].click();assert(els.components.innerHTML.includes('Studio components'));
assert(els.components.innerHTML.includes('Apply custom theme?'));
const oldOutbound=outbound.length;els['save-project'].click();assert.equal(outbound.length,oldOutbound+1);
let saved=outbound.at(-1);assert.equal(saved.action,'save-project');assert(saved.filename.endsWith('.phmiproject'));
const P=require('./project-model'),workspace=P.parse(saved.text);assert.equal(workspace.editor.view,'components');assert.deepEqual(workspace.theme,project());
els['open-project'].click();assert.equal(outbound.at(-1).action,'open-project');
const replacement=P.create(M.createBlank(),{...P.defaults,tab:'layout',snap:true,size:16});
context.ThemeMakerWorkspaceLoaded({document:replacement,filename:'blank.phmiproject'});assert.deepEqual(project(),replacement.theme);assert.equal(tabs[1].attrs['aria-selected'],'true');
const state=JSON.parse(storage.values().next().value);assert.deepEqual(state.editor,{grid:true,snap:true,size:16});
els.undo.click();assert.deepEqual(project(),workspace.theme);
const valid=project(),bad=M.copy(replacement);bad.theme.palette.text='invalid';context.ThemeMakerWorkspaceLoaded({document:bad,filename:'bad.phmiproject'});assert.deepEqual(project(),valid);assert(els.feedback.textContent.includes('Project rejected'));
console.log('PASS: Studio component gallery, native project Save/Open actions, restored workspace/grid, undo after opening and atomic invalid-project rejection');

// Color picking navigates editor controls without changing the package.
views[0].click();const pickPrior=project();els['pick-color'].click();assert.equal(els['pick-color'].attrs['aria-pressed'],'true');
assert(els.device.walk().find(e=>e.className==='studio-dock-hit').children.every(b=>b.disabled));
const buttonSurface=new Element('g');buttonSurface.setAttribute('data-theme-fill','panel');buttonSurface.setAttribute('data-theme-edge','accent');
const labelTarget=new Element('text');labelTarget.setAttribute('data-theme-ink','text');labelTarget.parentElement=buttonSurface;
function pickEvent(target){return {target:{closest:()=>target},preventDefault(){},stopPropagation(){}};}
els.device.children[0].onclick(pickEvent(labelTarget));assert.deepEqual(project(),pickPrior);
let selection=()=>els.editor.walk().find(e=>e.className==='picked-color');assert(selection());
assert.equal(tabs[0].attrs['aria-selected'],'true');assert.equal(selection().walk().filter(e=>e.tagName==='BUTTON'&&e.attrs['aria-pressed']!==undefined).length,3);
selection().walk().find(e=>e.textContent==='Fill').click();
let pickedHex=selection().walk().find(e=>e.className==='hex');assert.equal(pickedHex.attrs['aria-label'],'Panel hex');pickedHex.value='#123456';pickedHex.onchange();assert.equal(project().palette.panel,'#123456');
let reset=selection().walk().find(e=>e.className==='reset-color');reset.click();assert.equal(project().palette.panel,M.create('studio').palette.panel);els.undo.click();assert.equal(project().palette.panel,'#123456');
// Invalid text cannot reach an export, and resetting also clears the invalid edit.
pickedHex=selection().walk().find(e=>e.className==='hex');pickedHex.value='oops';pickedHex.onchange();assert(els.export.disabled);
reset=selection().walk().find(e=>e.className==='reset-color');reset.click();assert(!els.export.disabled);assert.equal(project().palette.panel,M.create('studio').palette.panel);
views[1].click();const lineTarget=new Element('path');lineTarget.setAttribute('data-theme-edge','border');els.components.onclick(pickEvent(lineTarget));assert(selection().walk().some(e=>e.attrs['aria-label']==='Border hex'));
const untouched=project(),foreignTarget=new Element('rect');foreignTarget.setAttribute('data-theme-fill','__proto__');els.components.onclick(pickEvent(foreignTarget));assert.deepEqual(project(),untouched);
selection().walk().find(e=>e.textContent==='Clear selection').click();assert(!selection());
els['pick-color'].click();assert.equal(els['pick-color'].attrs['aria-pressed'],'false');els.components.onclick(pickEvent(lineTarget));assert(!selection());
assert(!Object.hasOwn(project(),'picked'));assert(!Object.hasOwn(project(),'pickMode'));
els['pick-color'].click();let escaped=false;documentHandlers.keydown({key:'Escape',preventDefault(){escaped=true;}});assert(escaped);assert.equal(els['pick-color'].attrs['aria-pressed'],'false');assert(els.device.walk().find(e=>e.className==='studio-dock-hit').children.every(b=>!b.disabled));
els.workspace.click();els['blank-theme'].click();assert(els['pick-color'].disabled);
console.log('PASS: preview/component color picking, text/fill/border roles, per-color reset and undo, invalid-hex recovery, unknown-token rejection, mode exit, legacy restrictions and clean exports');

assert(els['compare-original'].disabled,'comparison is Studio-only');
els.workspace.click();els['starter-grid'].children[0].click();
const comparisonTheme=M.create('studio');comparisonTheme.palette.background='#223355';comparisonTheme.palette.text='#ABCDEF';comparisonTheme.metrics.radius_button=3;
comparisonTheme.schema_version=3;comparisonTheme.background={asset:'bg-'+'b'.repeat(64)+'.phmibg',fit:'cover',position_x:40,position_y:60,overlay_color:'#000000',overlay_opacity:25};
context.ThemeMakerWorkspaceLoaded({document:P.create(comparisonTheme),filename:'compare.phmiproject'});
const comparisonArtwork=outbound.at(-1);assert.equal(comparisonArtwork.action,'preview-artwork');context.ThemeMakerArtworkPreview({key:comparisonArtwork.key,data_url:'data:image/png;base64,AAAA'});
const customBefore=project(),storedBefore=storage.values().next().value,Scene=require('./studio-scene');
assert.equal(els.device.children[0].innerHTML,Scene.render(customBefore,'dashboard',{artwork:'data:image/png;base64,AAAA'}));
els['pick-color'].click();els['compare-original'].click();assert.equal(els['compare-original'].attrs['aria-pressed'],'true');assert.equal(els['pick-color'].attrs['aria-pressed'],'false');assert(els['pick-color'].disabled);assert(!els['comparison-banner'].hidden);
assert.equal(els.device.children[0].innerHTML,Scene.render(M.create('studio'),'dashboard',{artwork:''}));assert.deepEqual(project(),customBefore);assert.equal(storage.values().next().value,storedBefore);
els.export.click();assert.deepEqual(JSON.parse(outbound.at(-1).text),customBefore,'export must use the custom design while comparing');
els['save-project'].click();assert.deepEqual(P.parse(outbound.at(-1).text).theme,customBefore,'project save must use the custom design while comparing');
els.pages.children.find(e=>e.textContent==='Files').click();assert.equal(els.device.children[0].innerHTML,Scene.render(M.create('studio'),'files',{artwork:''}));
views[1].click();assert.equal(els.components.innerHTML,Scene.renderComponents(M.create('studio')));
els['compare-original'].click();assert(els['comparison-banner'].hidden);assert.equal(els.components.innerHTML,Scene.renderComponents(customBefore));assert(!els['pick-color'].disabled);
els['compare-original'].click();const backgroundHex=els.editor.walk().find(e=>e.attrs['aria-label']==='Background hex');backgroundHex.value='#102030';backgroundHex.onchange();assert.equal(project().palette.background,'#102030');assert.equal(els['compare-original'].attrs['aria-pressed'],'false','an edit returns to the custom preview');
els.undo.click();assert.equal(project().palette.background,customBefore.palette.background);
els['compare-original'].click();views[2].click();assert(els['compare-original'].disabled);assert(els['comparison-banner'].hidden);assert(!Object.hasOwn(project(),'compareOriginal'));
console.log('PASS: stock Studio palette/shapes without artwork, comparison across pages/Components, unchanged draft/theme, custom exports and project saves, picker exclusion, edit-to-design return, undo and Package restrictions');

// Exercise the mounted tablet action path, not only the pure state model.
views[0].click();els.workspace.click();els['starter-grid'].children[0].click();
const tabletTheme=project(),traffic=outbound.length;
function tabletClick(action,value=null){const target=new Element('g');target.setAttribute('data-tablet-action',action);if(value!==null)target.setAttribute('data-tablet-value',value);els.device.children[0].onclick(pickEvent(target));}
tabletClick('pause');assert(els.device.children[0].innerHTML.includes('PAUSED'));tabletClick('pause');assert(els.device.children[0].innerHTML.includes('PRINTING'));
tabletClick('stop');assert(els.device.children[0].innerHTML.includes('Stop print?'));assert(els.device.walk().find(e=>e.className==='studio-dock-hit').hidden,'navigation container must not intercept the dialog footer');tabletClick('close');assert(!els.device.children[0].innerHTML.includes('Stop print?'));assert(!els.device.walk().find(e=>e.className==='studio-dock-hit').hidden);
tabletClick('nav:files');tabletClick('file:0');assert(els.device.children[0].innerHTML.includes('Layer height: 0.20 mm'));tabletClick('file-print');assert(els.device.children[0].innerHTML.includes('Start print?'));tabletClick('confirm-print');assert.equal(els['preview-label'].textContent,'DASHBOARD');
tabletClick('nav:settings');let tabletScreen=els.device.children[0];tabletScreen.getBoundingClientRect=()=>({left:0,top:0,width:1024,height:600});tabletScreen.onwheel({clientX:300,clientY:200,deltaY:440,preventDefault(){}});assert(els.device.children[0].innerHTML.includes('translate(0 -374)'));
tabletClick('nav:console');tabletClick('command');let commandInput=els.device.walk().find(e=>e.className==='tablet-input');assert(commandInput);commandInput.value='G28';commandInput.oninput();tabletClick('command-apply');assert(els.device.children[0].innerHTML.includes('G28'));assert(els.device.children[0].innerHTML.includes('ok · simulated'));
tabletClick('nav:files');tabletClick('files-search');tabletClick('key','c');tabletClick('key','u');tabletClick('key','b');tabletClick('key','e');commandInput=els.device.walk().find(e=>e.className==='tablet-input');assert.equal(commandInput.value,'cube');tabletClick('files-search-apply');assert(!els.device.children[0].innerHTML.includes('Tool tray.gcode'));
tabletClick('nav:camera');tabletClick('fullscreen');assert(els.device.children[0].innerHTML.includes('Exit fullscreen'));documentHandlers.keydown({key:'Escape',preventDefault(){}});assert(!els.device.children[0].innerHTML.includes('Exit fullscreen'));
assert.deepEqual(project(),tabletTheme);assert.equal(outbound.length,traffic,'tablet interactions must never invoke native I/O or printer transport');
console.log('PASS: mounted tablet pause/confirm/navigation, modal ownership, file details/start, native viewport scroll, physical and on-screen command entry, search, fullscreen/Escape and zero native traffic or project mutation');

// All footer modes hide the whole dock, including its transparent flex container.
for(const action of ['stop','toolhead','temperature:nozzle','setting:about','fullscreen']){
 tabletClick(action);assert(els.device.walk().find(e=>e.className==='studio-dock-hit').hidden);
 tabletClick('close');assert(!els.device.walk().find(e=>e.className==='studio-dock-hit').hidden);
}
const outlined=Scene.render(M.create('studio'),'dashboard',{state:require('./tablet-model').dispatch(require('./tablet-model').create(),'stop')});
assert(/data-tablet-action="close"[^>]*><rect[^>]*pointer-events="all"/.test(outlined),'transparent interiors must receive pointer hits');
console.log('PASS: modal/fullscreen dock container isolation, Close restores navigation, outlined button interior hit areas');
// Exercise the HTML slider and nested editor return, not just pure model actions.
tabletClick('tune');let sliders=els.device.walk().filter(e=>e.className==='tablet-slider');assert.equal(sliders.length,2);sliders[0].value='135';sliders[0].oninput();sliders[0].onchange();assert(els.device.children[0].innerHTML.includes('135%'));tabletClick('close');
tabletClick('temperature:nozzle');assert(!els.device.walk().some(e=>e.className==='tablet-input'));tabletClick('custom:nozzle');assert(els.device.walk().some(e=>e.className==='tablet-input'));tabletClick('back-control');assert(els.device.children[0].innerHTML.includes('Choose a preset'));tabletClick('close');
assert.deepEqual(project(),tabletTheme);assert.equal(outbound.length,traffic);
console.log('PASS: mounted Tune slider updates and custom entry/Back without project or native I/O changes');
// The native bridge sends only fixture input, preserves theme data and ignores stale frames.
const nativeBaseline=project();context.ThemeMakerNativeStatus({available:true});
tabletClick('temperature:nozzle');let request=outbound.at(-1);assert.equal(request.action,'native-ui');assert.equal(request.kind,'nozzle');const token=request.token;
assert(els.device.walk().some(e=>e.className==='native-tablet-overlay'));
context.ThemeMakerNativeFrame({token,closed:false,image:'data:image/png;base64,AA==',nozzle:210,bed:60,fan:65,position:[110,110,12.8],zOffset:0});
context.ThemeMakerNativeFrame({token,closed:true,nozzle:220,bed:60,fan:65,command:'M104 S220'});
assert(!els.device.walk().some(e=>e.className==='native-tablet-overlay'));
tabletClick('fan');const pending=outbound.at(-1).token;tabletClick('nav:files');context.ThemeMakerNativeFrame({token:pending,closed:true,nozzle:999,bed:60,fan:65});assert.equal(els['preview-label'].textContent,'FILES');assert.deepEqual(project(),nativeBaseline);
context.ThemeMakerNativeStatus({available:false});
console.log('PASS: native availability, supported-popup routing, frame lifecycle, delayed-frame isolation and unchanged theme data');
// The native Tools page routes only explicit firmware actions back into the editor.
context.ThemeMakerNativeStatus({available:true});tabletClick('nav:tools');context.ThemeMakerNativeStatus({available:true});const toolsRequest=outbound.at(-1);assert.equal(toolsRequest.kind,'tools-page');
context.ThemeMakerNativeFrame({token:toolsRequest.token,closed:false,command:'@nav:printer',nozzle:210,bed:60,fan:65});assert.equal(els['preview-label'].textContent,'PRINTER');assert(!els.device.walk().some(e=>e.className==='native-tablet-overlay'));
context.ThemeMakerNativeStatus({available:false});tabletClick('nav:settings');let brightness=els.device.walk().find(e=>e.className==='tablet-brightness');assert(brightness);brightness.value='85';brightness.oninput();brightness.onchange();assert(els.device.children[0].innerHTML.includes('85%'));
assert.deepEqual(project(),nativeBaseline);
console.log('PASS: native Tools shell navigation and slider-only Settings brightness preserve the theme');

context.ThemeMakerNativeStatus({available:true});tabletClick('nav:tools');context.ThemeMakerNativeStatus({available:true});
let hoverLayer=els.device.walk().find(e=>e.className==='native-tablet-overlay');
hoverLayer.getBoundingClientRect=()=>({left:0,top:0,width:1024,height:600});
hoverLayer.onpointerenter({clientX:800,clientY:350});let hoverRequest=outbound.at(-1);
assert.equal(hoverRequest.operation,'hover');assert.equal(hoverRequest.x,800);
context.ThemeMakerNativeHover({...hoverRequest,cursor:'pointer'});assert.equal(hoverLayer.style.cursor,'pointer');
hoverLayer.onpointerleave();context.ThemeMakerNativeHover({...hoverRequest,cursor:'pointer'});assert.equal(hoverLayer.style.cursor,'default','late hover after leave is ignored');
hoverLayer.onpointerenter({clientX:50,clientY:40});hoverRequest=outbound.at(-1);
context.ThemeMakerNativeHover({...hoverRequest,cursor:'default'});assert.equal(hoverLayer.style.cursor,'default');
const oldHover=hoverRequest;tabletClick('nav:console');context.ThemeMakerNativeStatus({available:true});
assert.equal(outbound.at(-1).kind,'console-page');hoverLayer=els.device.walk().find(e=>e.className==='native-tablet-overlay');
context.ThemeMakerNativeHover({...oldHover,cursor:'pointer'});assert.equal(hoverLayer.style.cursor,'default','old page response is ignored');
assert.equal(hoverLayer.attrs['aria-label'],'Native firmware Console page');assert.deepEqual(project(),nativeBaseline);
console.log('PASS: native widget hover cursor, pointer-leave and stale-response isolation, native Console routing and unchanged theme');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:printer');context.ThemeMakerNativeStatus({available:true});
const printerRequest=outbound.at(-1);assert.equal(printerRequest.kind,'printer-page');assert.equal(printerRequest.job,'Spool holder.gcode');
context.ThemeMakerNativeFrame({token:printerRequest.token,closed:false,image:'data:image/png;base64,AA==',printState:'paused',nozzle:200,bed:60,fan:65});
context.ThemeMakerNativeFrame({token:printerRequest.token,closed:false,command:'@nav:tools',printState:'paused',nozzle:200,bed:60,fan:65});
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).printState,'paused','native print state carries to another page');
context.ThemeMakerNativeFrame({token:printerRequest.token,closed:false,printState:'printing',nozzle:999,bed:60,fan:65});
assert.equal(outbound.at(-1).printState,'paused','retired Printer frame cannot change simulation');assert.deepEqual(project(),nativeBaseline);
console.log('PASS: native Printer routing, job/state seed, Pause across pages, retired-frame isolation and unchanged theme');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:files');tabletClick('file:2');tabletClick('file-print');tabletClick('confirm-print');
context.ThemeMakerNativeStatus({available:true});let dashboardRequest=outbound.at(-1);
assert.equal(dashboardRequest.kind,'dashboard-page');assert.equal(dashboardRequest.progress,0);assert.equal(dashboardRequest.job,'Cable organizer.gcode');
assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Dashboard page');
context.ThemeMakerNativeFrame({token:dashboardRequest.token,closed:false,image:'data:image/png;base64,AA==',printState:'paused',progress:37.5,nozzle:200,bed:60,fan:65});
context.ThemeMakerNativeFrame({token:dashboardRequest.token,closed:false,command:'@nav:printer',printState:'paused',progress:37.5,nozzle:200,bed:60,fan:65});
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).kind,'printer-page');assert.equal(outbound.at(-1).progress,37.5);assert.equal(outbound.at(-1).printState,'paused');
assert.deepEqual(project(),nativeBaseline);
console.log('PASS: native Dashboard startup after file confirmation, zero progress/job seed, progress and Pause across native pages, unchanged theme');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:files');context.ThemeMakerNativeStatus({available:true});
const filesRequest=outbound.at(-1);assert.equal(filesRequest.kind,'files-page');
context.ThemeMakerNativeFrame({token:filesRequest.token,closed:false,image:'files',nozzle:210,bed:60,fan:65});
const filesLayer=els.device.walk().find(e=>e.className==='native-tablet-overlay');filesLayer.getBoundingClientRect=()=>({left:0,top:0,width:1024,height:600});
filesLayer.onwheel({preventDefault(){},stopPropagation(){},clientX:300,clientY:380,deltaY:3,deltaMode:1});
assert.equal(outbound.at(-1).operation,'wheel');assert.equal(outbound.at(-1).delta,72);assert.equal(outbound.at(-1).x,300);
context.ThemeMakerNativeFrame({token:filesRequest.token,closed:false,command:'@nav:dashboard',printState:'printing',progress:0,job:'Tool tray.gcode',nozzle:200,bed:60,fan:65});
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).kind,'dashboard-page');assert.equal(outbound.at(-1).job,'Tool tray.gcode');assert.equal(outbound.at(-1).progress,0);
const afterFiles=outbound.length;filesLayer.onwheel({preventDefault(){},stopPropagation(){},clientX:300,clientY:380,deltaY:100,deltaMode:0});
filesLayer.onpointerenter({clientX:300,clientY:380});assert.equal(outbound.length,afterFiles,'retired overlay cannot send wheel or hover to the new page');
assert.deepEqual(project(),nativeBaseline);
console.log('PASS: native Files routing, wheel unit normalization, confirmed job/progress into Dashboard, stale-overlay isolation and unchanged theme');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:camera');context.ThemeMakerNativeStatus({available:true});
const cameraRequest=outbound.at(-1);assert.equal(cameraRequest.kind,'camera-page');
assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Camera page');
context.ThemeMakerNativeFrame({token:cameraRequest.token,closed:false,command:'@nav:dashboard',printState:'printing',progress:0,job:'Tool tray.gcode',nozzle:200,bed:60,fan:65});
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).kind,'dashboard-page');
const cameraAfter=outbound.length;context.ThemeMakerNativeFrame({token:cameraRequest.token,closed:false,command:'@nav:camera',nozzle:999,bed:60,fan:65});
assert.equal(outbound.length,cameraAfter);assert.equal(els['preview-label'].textContent,'DASHBOARD');assert.deepEqual(project(),nativeBaseline);
console.log('PASS: native Camera routing, native shell return, stale-frame isolation and unchanged theme');

const resetBefore=project();els['reset-tablet'].click();assert.equal(outbound.at(-1).operation,'reset');assert.deepEqual(project(),resetBefore);
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).progress,64);assert.equal(outbound.at(-1).job,'Spool holder.gcode');
console.log('PASS: Reset tablet retires the native fixture process, restores sample machine state and preserves the theme');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:drybox');context.ThemeMakerNativeStatus({available:true});
const dryboxRequest=outbound.at(-1);assert.equal(dryboxRequest.kind,'drybox-page');
assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Drybox page');
context.ThemeMakerNativeFrame({token:dryboxRequest.token,closed:false,command:'@nav:tools',printState:'printing',progress:64,job:'Spool holder.gcode',nozzle:210,bed:60,fan:65});
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).kind,'tools-page');
const afterDrybox=outbound.length;context.ThemeMakerNativeFrame({token:dryboxRequest.token,closed:false,command:'@nav:drybox',nozzle:999});
assert.equal(outbound.length,afterDrybox);assert.equal(els['preview-label'].textContent,'TOOLS');assert.deepEqual(project(),resetBefore);
console.log('PASS: native Drybox routing, native shell navigation, stale-frame isolation and unchanged theme');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:settings');context.ThemeMakerNativeStatus({available:true});
let settingsRequest=outbound.at(-1);assert.equal(settingsRequest.kind,'settings-page');
assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Settings page');
const settingsTheme=project();context.ThemeMakerNativeFrame({token:settingsRequest.token,closed:false,command:'@setting:ota',nozzle:210,bed:60,fan:65});
assert(els.device.children[0].innerHTML.includes('OTA update'),'unmigrated OTA uses the existing fixture dialog');
tabletClick('close');context.ThemeMakerNativeStatus({available:true});settingsRequest=outbound.at(-1);assert.equal(settingsRequest.kind,'settings-page');
context.ThemeMakerNativeFrame({token:settingsRequest.token,closed:false,command:'@native-reset',nozzle:210,bed:60,fan:65});
assert.equal(outbound.at(-1).operation,'reset');assert.deepEqual(project(),settingsTheme);
context.ThemeMakerNativeStatus({available:true});settingsRequest=outbound.at(-1);assert.equal(settingsRequest.kind,'settings-page');
context.ThemeMakerNativeFrame({token:settingsRequest.token,closed:false,command:'@nav:network',nozzle:210,bed:60,fan:65});
assert.equal(els['preview-label'].textContent,'NETWORK');assert.deepEqual(project(),settingsTheme);
console.log('PASS: native Settings routing, OTA fallback, factory-reset process retirement, Network navigation and unchanged theme');

context.ThemeMakerNativeStatus({available:true});const networkRequest=outbound.at(-1);assert.equal(networkRequest.kind,'network-page');
assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Network page');
context.ThemeMakerNativeFrame({token:networkRequest.token,closed:false,command:'@printers',nozzle:210,bed:60,fan:65});
assert.equal(outbound.at(-1).kind,'chooser-page','printer actions route to the native chooser');
const chooserRequest=outbound.at(-1);
assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Printers page');
context.ThemeMakerNativeFrame({token:chooserRequest.token,closed:false,command:'@nav:network',nozzle:210,bed:60,fan:65});
context.ThemeMakerNativeStatus({available:true});const networkReturn=outbound.at(-1);assert.equal(networkReturn.kind,'network-page');
context.ThemeMakerNativeFrame({token:networkReturn.token,closed:false,command:'@nav:settings',nozzle:210,bed:60,fan:65});
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).kind,'settings-page');
const networkAfter=outbound.length;context.ThemeMakerNativeFrame({token:networkRequest.token,closed:false,command:'@nav:network',nozzle:999});
assert.equal(outbound.length,networkAfter);assert.equal(els['preview-label'].textContent,'SETTINGS');assert.deepEqual(project(),settingsTheme);
console.log('PASS: native Network startup from Settings, native chooser return, native Settings navigation, stale-frame isolation and unchanged theme');

context.ThemeMakerNativeFrame({token:outbound.at(-1).token,closed:false,command:'@printers',nozzle:210,bed:60,fan:65});
const profileChooser=outbound.at(-1);assert.equal(profileChooser.kind,'chooser-page');
context.ThemeMakerNativeFrame({token:profileChooser.token,closed:false,command:'@nav:dashboard',printer:'Sermoon D1 #2',job:'',progress:0,printState:'standby',nozzle:0,bed:0,fan:0});
context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).kind,'dashboard-page');assert.equal(outbound.at(-1).job,'');assert.equal(outbound.at(-1).progress,0);assert.equal(outbound.at(-1).printState,'standby');
const profileAfter=outbound.length;context.ThemeMakerNativeFrame({token:profileChooser.token,closed:false,command:'@nav:printer',printer:'Old printer',job:'old.gcode',progress:99,printState:'printing',nozzle:999});
assert.equal(outbound.length,profileAfter);assert.deepEqual(project(),settingsTheme);
console.log('PASS: native profile selection carries printer identity, empty job, idle state and zero targets before Dashboard navigation; retired frames and project data stay isolated');

// A slow native response must not build a queue of idle frames or wheel requests.
tabletClick('nav:settings');context.ThemeMakerNativeStatus({available:true});
const scrollOpen=outbound.at(-1),scrollFrame={token:scrollOpen.token,closed:false,image:'scroll-a',nozzle:0,bed:0,fan:0};
let scrollLayer=els.device.walk().find(e=>e.className==='native-tablet-overlay');scrollLayer.getBoundingClientRect=()=>({left:0,top:0,width:1024,height:600});
let clock=1000;context.Date={now:()=>clock};
const wheel=delta=>scrollLayer.onwheel({preventDefault(){},stopPropagation(){},clientX:500,clientY:350,deltaY:delta,deltaMode:0});
let trafficBefore=outbound.length;wheel(400);wheel(400);intervals.at(-1)();
assert.equal(outbound.length,trafficBefore,'opening frame absorbs wheel burst and suppresses idle polls');
context.ThemeMakerNativeFrame(scrollFrame);assert.equal(outbound.at(-1).operation,'wheel');assert.equal(outbound.at(-1).delta,600);
trafficBefore=outbound.length;clock+=20;wheel(100);intervals.at(-1)();assert.equal(outbound.length,trafficBefore,'in-flight wheel absorbs more input');
context.ThemeMakerNativeFrame({...scrollFrame,image:'scroll-b'});assert.equal(outbound.at(-1).delta,300,'all remaining wheel distance survives coalescing');
trafficBefore=outbound.length;context.ThemeMakerNativeFrame({...scrollFrame,image:'scroll-c'});assert.equal(outbound.length,trafficBefore,'empty wheel queue adds no request');
intervals.at(-1)();assert.equal(outbound.at(-1).operation,'frame');trafficBefore=outbound.length;intervals.at(-1)();assert.equal(outbound.length,trafficBefore,'only one idle frame can be in flight');
context.ThemeMakerNativeFrame({...scrollFrame,image:'scroll-c'});
clock+=20;wheel(.4);clock+=20;wheel(.4);trafficBefore=outbound.length;clock+=20;wheel(.4);assert.equal(outbound.length,trafficBefore+1);assert.equal(outbound.at(-1).delta,1,'fractional touchpad distance accumulates');
context.ThemeMakerNativeFrame({...scrollFrame,image:'scroll-d'});
clock+=20;wheel(100);clock+=20;wheel(1500);trafficBefore=outbound.length;
scrollLayer.onpointerdown({preventDefault(){},stopPropagation(){},clientX:500,clientY:350,pointerId:1,button:0});
assert.deepEqual(outbound.slice(trafficBefore).map(r=>r.operation),['wheel','wheel','wheel','pointer'],'queued wheel distance is sent before the following press');
assert.deepEqual(project(),nativeBaseline);
console.log('PASS: bounded in-flight wheel/idle traffic, lossless wheel bursts, fractional touchpad input and scroll-before-click ordering');

context.ThemeMakerNativeFrame({...scrollFrame,image:'scroll-e'}); // one of the pending scroll frames
// Enter a fresh page so the drag fixture starts without prior input responses.
tabletClick('nav:console');context.ThemeMakerNativeStatus({available:true});
const dragOpen=outbound.at(-1),dragFrame={token:dragOpen.token,closed:false,image:'drag-a',nozzle:0,bed:0,fan:0};
context.ThemeMakerNativeFrame(dragFrame);
const dragLayer=els.device.walk().find(e=>e.className==='native-tablet-overlay');dragLayer.getBoundingClientRect=()=>({left:0,top:0,width:1024,height:600});
const dragEvent=y=>({preventDefault(){},stopPropagation(){},clientX:500,clientY:y,pointerId:1,button:0});
dragLayer.onpointerdown(dragEvent(400));trafficBefore=outbound.length;
clock+=20;dragLayer.onpointermove(dragEvent(370));clock+=20;dragLayer.onpointermove(dragEvent(340));assert.equal(outbound.length,trafficBefore,'in-flight press coalesces drag movement');
context.ThemeMakerNativeFrame({...dragFrame,image:'drag-b'});assert.equal(outbound.at(-1).operation,'pointer');assert.equal(outbound.at(-1).y,340);assert.equal(outbound.at(-1).down,1);
clock+=20;dragLayer.onpointermove(dragEvent(310));dragLayer.onpointerup(dragEvent(290));
assert.equal(outbound.at(-3).down,1);assert.equal(outbound.at(-3).y,290);assert.equal(outbound.at(-2).down,0);assert.equal(outbound.at(-2).y,290,'trailing drag position precedes release');
console.log('PASS: drag backpressure retains the latest position and delivers it before release');

// Telemetry joins the native page lifecycle and returns to its Devices owner.
context.ThemeMakerNativeStatus({available:false});tabletClick('nav:telemetry');context.ThemeMakerNativeStatus({available:true});
const telemetryOpen=outbound.at(-1);assert.equal(telemetryOpen.kind,'telemetry-page');
assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Telemetry page');
context.ThemeMakerNativeFrame({token:telemetryOpen.token,closed:false,command:'@tool:Devices',nozzle:210,bed:60,fan:65});
assert.equal(outbound.at(-1).kind,'devices-page');assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Devices page');tabletClick('nav:dashboard');
const afterTelemetry=outbound.length;context.ThemeMakerNativeFrame({token:telemetryOpen.token,closed:false,command:'@nav:settings',nozzle:999,bed:999,fan:99});
assert.equal(els['preview-label'].textContent,'DASHBOARD');assert.equal(outbound.length,afterTelemetry);assert.deepEqual(project(),nativeBaseline);
console.log('PASS: native Telemetry routing, Devices return, stale-frame isolation and unchanged project');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:tools');context.ThemeMakerNativeStatus({available:true});
const nativeTools=outbound.at(-1);context.ThemeMakerNativeFrame({token:nativeTools.token,closed:false,command:'@tool:Devices',nozzle:210,bed:60,fan:65});
const nativeDevices=outbound.at(-1);assert.equal(nativeDevices.kind,'devices-page');
context.ThemeMakerNativeFrame({token:nativeDevices.token,closed:false,command:'@nav:telemetry',nozzle:210,bed:60,fan:65});context.ThemeMakerNativeStatus({available:true});assert.equal(outbound.at(-1).kind,'telemetry-page');
const afterGraphs=outbound.length;context.ThemeMakerNativeFrame({token:nativeDevices.token,closed:false,command:'@nav:settings',nozzle:999,bed:999,fan:99});assert.equal(outbound.length,afterGraphs);assert.equal(els['preview-label'].textContent,'TELEMETRY');assert.deepEqual(project(),nativeBaseline);
console.log('PASS: Tools opens native Devices, Graphs opens native Telemetry, retired Devices frames stay isolated');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:tools');context.ThemeMakerNativeStatus({available:true});
const macroTools=outbound.at(-1);context.ThemeMakerNativeFrame({token:macroTools.token,closed:false,command:'@tool:Macros',nozzle:210,bed:60,fan:65});
const macroOpen=outbound.at(-1);assert.equal(macroOpen.kind,'macros-page');assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Macros page');
context.ThemeMakerNativeFrame({token:macroOpen.token,closed:false,command:'@nav:console',nozzle:210,bed:60,fan:65});assert.equal(els['preview-label'].textContent,'CONSOLE');
const afterMacros=outbound.length;context.ThemeMakerNativeFrame({token:macroOpen.token,closed:false,command:'@nav:settings',nozzle:999,bed:999,fan:99});assert.equal(outbound.length,afterMacros);assert.equal(els['preview-label'].textContent,'CONSOLE');assert.deepEqual(project(),nativeBaseline);
console.log('PASS: Tools opens native Macros, shell routes return to pages and retired frames leave project/state unchanged');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:tools');context.ThemeMakerNativeStatus({available:true});
const meshTools=outbound.at(-1);context.ThemeMakerNativeFrame({token:meshTools.token,closed:false,command:'@tool:Bed mesh',nozzle:210,bed:60,fan:65});
const meshOpen=outbound.at(-1);assert.equal(meshOpen.kind,'mesh-page');const meshLayer=els.device.walk().find(e=>e.className==='native-tablet-overlay');assert.equal(meshLayer.attrs['aria-label'],'Native firmware Bed mesh page');meshLayer.getBoundingClientRect=()=>({left:0,top:0,width:1024,height:600});
meshLayer.onpointerenter({clientX:500,clientY:250});const meshHover=outbound.at(-1);context.ThemeMakerNativeHover({...meshHover,cursor:'grab'});assert.equal(meshLayer.style.cursor,'grab');context.ThemeMakerNativeHover({...meshHover,cursor:'grabbing'});assert.equal(meshLayer.style.cursor,'grabbing');
context.ThemeMakerNativeFrame({token:meshOpen.token,closed:false,command:'@nav:dashboard',nozzle:210,bed:60,fan:65});assert.equal(els['preview-label'].textContent,'DASHBOARD');const afterMesh=outbound.length;context.ThemeMakerNativeFrame({token:meshOpen.token,closed:false,command:'@nav:settings',nozzle:999,bed:999,fan:99});assert.equal(outbound.length,afterMesh);assert.deepEqual(project(),nativeBaseline);
console.log('PASS: Tools opens native Bed mesh, grab cursors and shell routing work, retired frames preserve state/project');

context.ThemeMakerNativeStatus({available:false});tabletClick('nav:tools');context.ThemeMakerNativeStatus({available:true});
const calibrationTools=outbound.at(-1);context.ThemeMakerNativeFrame({token:calibrationTools.token,closed:false,command:'@tool:Calibration',nozzle:210,bed:60,fan:65});
const calibrationOpen=outbound.at(-1);assert.equal(calibrationOpen.kind,'calibration-page');assert.equal(els.device.walk().find(e=>e.className==='native-tablet-overlay').attrs['aria-label'],'Native firmware Calibration page');
context.ThemeMakerNativeFrame({token:calibrationOpen.token,closed:false,command:'@tool:Bed mesh',nozzle:210,bed:60,fan:65});assert.equal(outbound.at(-1).kind,'mesh-page');
const afterCalibration=outbound.length;context.ThemeMakerNativeFrame({token:calibrationOpen.token,closed:false,command:'@nav:settings',nozzle:999,bed:999,fan:99});assert.equal(outbound.length,afterCalibration);assert.deepEqual(project(),nativeBaseline);
console.log('PASS: Tools opens native Calibration, Bed mesh routes natively, retired frames leave project/state unchanged');
