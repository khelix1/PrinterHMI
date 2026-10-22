'use strict';
const assert=require('node:assert/strict');
const M=require('./theme-model.js');
const rejects=p=>assert.throws(()=>M.serialize(p));
for(const base of ['studio','operator','foundry','glass']){
 const p=M.create(base);assert.deepEqual(M.validate(p),[]);
 assert.deepEqual(M.parse(M.serialize(p)),p);
 p.author='';p.description='';assert(!('author' in M.parse(M.serialize(p))));
 for(const key of Object.keys(p.palette)){const q=M.copy(p);delete q.palette[key];rejects(q);}
 for(const key of Object.keys(p.metrics)){const q=M.copy(p);q.metrics[key]=37.5;rejects(q);}
 const q=M.copy(p);q.name='😀'.repeat(11);rejects(q);q.name='A\u0000B';rejects(q);
 const bad=M.copy(p);bad.palette.background='url(https://invalid)';rejects(bad);
 const other=M.copy(p);other.artwork='missing.png';rejects(other);
 if(base!=='studio'){
  for(const page of Object.keys(p.layouts)){const q=M.copy(p);delete q.layouts[page];rejects(q);}
  p.layouts.dashboard.active_print.x=814;p.layouts.dashboard.active_print.width=854;rejects(p);
 }else{
  const q=M.copy(p);q.layouts={};rejects(q);delete q.layouts;q.metrics.surface_opacity=128;rejects(q);
  q.metrics.surface_opacity=255;q.schema_version=1;rejects(q);
 }
}
assert.throws(()=>M.parse('['.repeat(17)+']'.repeat(17)));
assert.throws(()=>M.parse(' '.repeat(49153)));
assert.throws(()=>M.parse(M.serialize(M.create())+'\0'));
assert.throws(()=>M.parse(M.serialize(M.create())+' trailing'));
assert.equal(M.contrast('#000000','#FFFFFF'),21);
assert.equal(M.contrast('#444444','#444444'),1);
console.log('PASS: all four bases round-trip; strict schema, palette, metrics, UTF-8, geometry, size, nesting and contrast checks');
const blank=M.createBlank();assert.deepEqual(M.validate(blank),[]);assert.deepEqual(M.parse(M.serialize(blank)),blank);
assert(Object.values(blank.layouts.dashboard).every(r=>r.visible===false));assert(blank.layouts.settings.content.visible);
for(const [page,names]of Object.entries(M.spec.regions)){
 if(page==='telemetry')continue;
 for(const name of names){const added=M.setRegion(blank,page,name,true);assert(added.layouts[page][name].visible);assert.deepEqual(M.validate(added),[]);assert.equal(M.setRegion(added,page,name,false).layouts[page][name].visible,false);}
}
assert.throws(()=>M.setRegion(blank,'dashboard','made_up',true));assert.throws(()=>M.setRegion(blank,'telemetry','charts',false));
assert.throws(()=>M.setRegion(M.create('studio'),'dashboard','active_print',true));
assert(Object.values(blank.layouts.dashboard).every(r=>!r.visible),'block edits must not mutate the original');
console.log('PASS: blank theme, independent neutral palette, bounded block add/remove, non-mutating edits and firmware-owned page restrictions');
for(const grid of [1,4,8,16,32]){
 for(const resize of [false,true])for(const delta of [-10000,-17,3,10000]){
  const r=M.moveRect(blank.layouts.dashboard.active_print,delta,delta,resize,grid);
  const q=M.copy(blank);q.layouts.dashboard.active_print=r;assert.deepEqual(M.validate(q),[]);
 }
}
for(const mode of ['left','center-x','right','top','center-y','bottom','fill-width','fill-height']){
 const q=M.alignRegion(blank,'dashboard','active_print',mode);assert.deepEqual(M.validate(q),[]);assert.deepEqual(M.parse(M.serialize(q)),q);
}
assert.equal(M.alignRegion(blank,'dashboard','active_print','right').layouts.dashboard.active_print.x,456);
assert.equal(M.alignRegion(blank,'dashboard','active_print','bottom').layouts.dashboard.active_print.y,224);
assert.throws(()=>M.alignRegion(blank,'dashboard','active_print','front'));
assert.throws(()=>M.moveRect(blank.layouts.dashboard.active_print,NaN,0));
assert.throws(()=>M.moveRect(blank.layouts.dashboard.active_print,0,0,false,0));
assert.deepEqual(M.overlaps(blank,'dashboard'),[]);
let pair=M.setRegion(blank,'dashboard','active_print',true);pair=M.setRegion(pair,'dashboard','machine_status',true);assert.deepEqual(M.overlaps(pair,'dashboard'),[]);
pair.layouts.dashboard.machine_status.x=16;assert.equal(M.overlaps(pair,'dashboard').length,1);
console.log('PASS: grid rounding, movement/resize bounds, alignment exports, invalid canvas operations and overlap detection');
for(const base of ['operator','studio']){
 const p=M.create(base);p.schema_version=3;p.background={asset:'bg-'+'a'.repeat(64)+'.phmibg',fit:'cover',position_x:50,position_y:50,overlay_color:'#000000',overlay_opacity:35};
 assert.deepEqual(M.validate(p),[]);assert.deepEqual(M.parse(M.serialize(p)),p);
 for(const key of Object.keys(p.background)){const q=M.copy(p);delete q.background[key];assert.throws(()=>M.serialize(q));}
 const q=M.copy(p);q.background.asset='../outside.phmibg';assert.throws(()=>M.serialize(q));
 const z=M.copy(p);z.background.overlay_opacity=101;assert.throws(()=>M.serialize(z));
 const old=M.copy(p);old.schema_version=base==='studio'?2:1;assert.throws(()=>M.serialize(old));
}
console.log('PASS: v3 artwork contracts for both renderer palettes, missing fields, path confinement and firmware-version requirements');
