/* Portable model: browser/file:// and Node tests share exactly the same rules. */
(function(g){
'use strict';
const S=typeof module!=='undefined'&&module.exports?require('./theme-spec.js'):g.ThemeSpec;
const copy=v=>JSON.parse(JSON.stringify(v));
const bytes=s=>new TextEncoder().encode(s).length;
const own=(o,k)=>Object.prototype.hasOwnProperty.call(o,k);
const record=o=>o!==null&&typeof o==='object'&&!Array.isArray(o);
function create(base='studio'){
 if(!own(S.palettes,base))throw new Error('Unknown base theme');
 const keys=base==='studio'?S.keys:S.legacyKeys;
 const p={schema:S.format,schema_version:base==='studio'?2:1,id:'my-'+base,name:'My '+(base==='studio'?'Studio':base[0].toUpperCase()+base.slice(1)),palette:{},metrics:copy(S.metrics[base]),base_theme:base};
 keys.forEach(k=>p.palette[k]=S.palettes[base][k]);
 if(base!=='studio')p.layouts=copy(S.layouts[base]);
 return p;
}
/* Blank designs supply their own neutral tokens and geometry. The base is a
 * firmware rendering contract, not a preset palette or copied composition. */
function createBlank(){
 const p=create('operator');p.id='untitled-theme';p.name='Untitled theme';
 const neutral={background:'#F3F3F0',background_deep:'#E9E9E5',popup:'#FFFFFF',topbar:'#FFFFFF',navigation:'#FFFFFF',panel:'#FFFFFF',panel_alt:'#EBEBE7',card:'#FFFFFF',card_dark:'#E9E9E5',control:'#E5E5E0',control_alt:'#F3F3F0',border:'#CECEC7',border_soft:'#DDDDD6',border_control:'#B8B8AF',text:'#242724',text_bright:'#111411',text_dim:'#555B55',text_muted:'#656C65',accent:'#435E50',accent_bright:'#334D40',success:'#267347',warning:'#8A5C08',danger:'#AE3333',telemetry_grid:'#DADAD3',telemetry_bed:'#996300',telemetry_chamber:'#78599B',telemetry_humidity:'#25768A'};
 p.palette=copy(neutral);p.metrics={radius_card:0,radius_button:0,radius_panel:0,surface_opacity:255};
 const rect=(x,y,width,height,visible=false)=>({x,y,width,height,visible});
 p.layouts={
 dashboard:{banner:rect(16,16,822,48),active_print:rect(16,80,398,304),machine_status:rect(430,80,398,304),command_bar:rect(16,404,822,64)},
 printer:{active:rect(16,16,822,240),status:rect(16,272,398,224),actions:rect(430,272,398,224)},
 drybox:{environment:rect(16,16,398,224),drying_system:rect(430,16,398,224),material_program:rect(16,256,822,240)},
 files:{breadcrumb:rect(16,16,822,40),up:rect(16,72,52,44),search:rect(84,72,398,44),sort:rect(498,72,190,44),refresh:rect(704,72,100,44),list:rect(16,132,822,364)},
 network:{wifi:rect(16,16,398,224),moonraker:rect(430,16,398,224),networks:rect(16,256,822,156),actions:rect(16,428,822,48)},
 settings:{banner:rect(16,16,822,48),content:rect(16,80,822,432,true)},
 telemetry:{metric_1:rect(16,16,190,72,true),metric_2:rect(220,16,190,72,true),metric_3:rect(424,16,190,72,true),metric_4:rect(628,16,190,72,true),charts:rect(16,112,822,384,true)}
 };return p;
}
function setRegion(p,page,key,visible){
 if(p.base_theme==='studio'||page==='telemetry')throw new Error('This page uses a firmware-owned composition.');
 if(!own(S.regions,page)||!S.regions[page].includes(key))throw new Error('Unknown printer block.');
 const next=copy(p);next.layouts[page][key].visible=Boolean(visible);
 const errors=validate(next);if(errors.length)throw new Error(errors[0]);return next;
}
function moveRect(r,dx,dy,resize=false,grid=1){
 if(!Number.isFinite(dx)||!Number.isFinite(dy)||![1,4,8,16,32].includes(grid))throw new Error('Invalid canvas movement.');
 const q={...r},snap=n=>Math.round(n/grid)*grid;
 if(resize){q.width=Math.max(40,Math.min(854-r.x,snap(r.width+dx)));q.height=Math.max(20,Math.min(528-r.y,snap(r.height+dy)));}
 else{q.x=Math.max(0,Math.min(854-r.width,snap(r.x+dx)));q.y=Math.max(0,Math.min(528-r.height,snap(r.y+dy)));}
 return q;
}
function alignRegion(p,page,key,alignment){
 const q=setRegion(p,page,key,p.layouts?.[page]?.[key]?.visible);const r=q.layouts[page][key];
 switch(alignment){
 case 'left':r.x=0;break;case 'center-x':r.x=Math.round((854-r.width)/2);break;case 'right':r.x=854-r.width;break;
 case 'top':r.y=0;break;case 'center-y':r.y=Math.round((528-r.height)/2);break;case 'bottom':r.y=528-r.height;break;
 case 'fill-width':r.x=16;r.width=822;break;case 'fill-height':r.y=16;r.height=496;break;
 default:throw new Error('Unknown alignment.');
 }return q;
}
function overlaps(p,page){
 if(!p.layouts||page==='telemetry')return [];
 const rows=Object.entries(p.layouts[page]).filter(([,r])=>r.visible),pairs=[];
 for(let i=0;i<rows.length;i++)for(let j=i+1;j<rows.length;j++){
  const [a,r]=rows[i],[b,s]=rows[j];if(r.x<s.x+s.width&&r.x+r.width>s.x&&r.y<s.y+s.height&&r.y+r.height>s.y)pairs.push([a,b]);
 }return pairs;
}
function validate(p){
 const errors=[];const e=(path,message)=>errors.push(path+': '+message);
 if(!record(p))return ['Theme must be a JSON object.'];
 for(const k of Object.keys(p))if(!['schema','schema_version','base_theme','id','name','author','description','palette','metrics','layouts','background'].includes(k))e(k,'unsupported field; import would discard data');
 if(p.schema!==S.format)e('schema','must be printerhmi.theme');
 if(![1,2,3].includes(p.schema_version))e('schema_version','must be 1, 2 or 3');
 const base=p.base_theme;
 if(!own(S.palettes,base))e('base_theme','choose Studio, Operator, Foundry or Glass');
 if(base==='studio'&&p.schema_version<2)e('schema_version','Studio requires version 2');
 if(typeof p.id!=='string'||!/^[a-z0-9][a-z0-9-]{1,31}$/.test(p.id))e('id','use 2–32 lowercase letters, numbers or hyphens');
 for(const [k,max,required] of [['name',40,true],['author',40,false],['description',120,false]]){
  if(!own(p,k)){if(required)e(k,'required');continue;}
  if(typeof p[k]!=='string'||bytes(p[k])>max||(required&&!p[k].length)||/[\u0000-\u001f\u007f]/.test(p[k]))e(k,'use '+(required?'1–':'0–')+max+' UTF-8 bytes without control characters');
 }
 const keys=p.schema_version===1||(p.schema_version===3&&base!=='studio')?S.legacyKeys:S.keys;
 if(!record(p.palette))e('palette','required object');
 else{
  keys.forEach(k=>{if(typeof p.palette[k]!=='string'||!/^#[0-9a-f]{6}$/i.test(p.palette[k]))e('palette.'+k,'use #RRGGBB');});
  for(const k of Object.keys(p.palette))if(!keys.includes(k))e('palette.'+k,'unsupported color for this format');
 }
 if(!record(p.metrics))e('metrics','required object');
 else{
  for(const [k,min,max] of [['radius_card',0,36],['radius_button',0,36],['radius_panel',0,36],['surface_opacity',96,255]])if(!Number.isInteger(p.metrics[k])||p.metrics[k]<min||p.metrics[k]>max)e('metrics.'+k,'use an integer '+min+'–'+max);
  for(const k of Object.keys(p.metrics))if(!['radius_card','radius_button','radius_panel','surface_opacity'].includes(k))e('metrics.'+k,'unsupported field');
  if(base==='studio'&&p.metrics.surface_opacity!==255)e('metrics.surface_opacity','Studio uses opaque surfaces (255)');
 }
 if(base==='studio'){if(own(p,'layouts'))e('layouts','Studio uses its native layout; omit legacy rectangles');}
 else if(!record(p.layouts))e('layouts','complete legacy layout required');
 else{
  for(const [page,names] of Object.entries(S.regions)){
   if(!record(p.layouts[page])){e('layouts.'+page,'required');continue;}
   for(const key of names){
    const r=p.layouts[page][key];const path='layouts.'+page+'.'+key;
    if(!record(r)){e(path,'required rectangle');continue;}
    for(const [k,min,max] of [['x',0,814],['y',0,508],['width',40,854],['height',20,528]])if(!Number.isInteger(r[k])||r[k]<min||r[k]>max)e(path+'.'+k,'outside firmware bounds');
    if(typeof r.visible!=='boolean')e(path+'.visible','must be true or false');
    if(r.x+r.width>854||r.y+r.height>528)e(path,'must fit inside 854 × 528');
    for(const k of Object.keys(r))if(!['x','y','width','height','visible'].includes(k))e(path+'.'+k,'unsupported field');
   }
   for(const k of Object.keys(p.layouts[page]))if(!names.includes(k))e('layouts.'+page+'.'+k,'unsupported region');
  }
  for(const k of Object.keys(p.layouts))if(!own(S.regions,k))e('layouts.'+k,'unsupported page');
 }
 if(p.schema_version===3){
  const bg=p.background;if(!record(bg))e('background','artwork settings required');
  else{
   if(typeof bg.asset!=='string'||!/^bg-[a-f0-9]{64}\.phmibg$/.test(bg.asset))e('background.asset','invalid artwork reference');
   if(!['cover','contain','stretch'].includes(bg.fit))e('background.fit','choose cover, contain or stretch');
   for(const key of ['position_x','position_y','overlay_opacity'])if(!Number.isInteger(bg[key])||bg[key]<0||bg[key]>100)e('background.'+key,'use an integer 0–100');
   if(typeof bg.overlay_color!=='string'||!/^#[a-f0-9]{6}$/i.test(bg.overlay_color))e('background.overlay_color','use #RRGGBB');
   for(const key of Object.keys(bg))if(!['asset','fit','position_x','position_y','overlay_color','overlay_opacity'].includes(key))e('background.'+key,'unsupported field');
  }
 }else if(own(p,'background'))e('background','artwork requires version 3 firmware');
 if(!errors.length&&bytes(JSON.stringify(p))>S.maxBytes)e('file','must be no larger than 48 KiB');
 return errors;
}
function clean(p){const q=copy(p);for(const k of ['author','description'])if(q[k]==='')delete q[k];return q;}
function serialize(p){const errors=validate(p);if(errors.length)throw new Error(errors[0]);const text=JSON.stringify(clean(p),null,2)+'\n';if(bytes(text)>S.maxBytes)throw new Error('Export exceeds 48 KiB');return text;}
function parse(text){
 if(typeof text!=='string'||bytes(text)>S.maxBytes)throw new Error('Import must be UTF-8 JSON, no larger than 48 KiB');
 let depth=0,quoted=false,escaped=false;
 for(const c of text){if(c==='\0')throw new Error('NUL bytes are not allowed');if(quoted){if(escaped)escaped=false;else if(c==='\\')escaped=true;else if(c==='"')quoted=false;}else if(c==='"')quoted=true;else if(c==='['||c==='{'){if(++depth>S.maxDepth)throw new Error('JSON is nested too deeply');}else if(c===']'||c==='}')depth--;}
 const p=JSON.parse(text);const errors=validate(p);if(errors.length)throw new Error(errors.slice(0,3).join('\n'));return clean(p);
}
function contrast(a,b){const lum=h=>{const rgb=h.slice(1).match(/../g).map(v=>parseInt(v,16)/255).map(v=>v<=.04045?v/12.92:((v+.055)/1.055)**2.4);return rgb[0]*.2126+rgb[1]*.7152+rgb[2]*.0722;};const x=lum(a),y=lum(b);return (Math.max(x,y)+.05)/(Math.min(x,y)+.05);}
const api={spec:S,create,createBlank,setRegion,moveRect,alignRegion,overlaps,validate,serialize,parse,clean,copy,bytes,contrast};
if(typeof module!=='undefined'&&module.exports)module.exports=api;else g.ThemeModel=api;
})(globalThis);
