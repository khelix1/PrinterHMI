'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const M=require('./theme-model'),Scene=require('./studio-scene');
const p=M.create('studio'),before=M.serialize(p),svg=Scene.render(p);
assert.equal(Scene.rgb565('#15161B'),'#101418');assert.equal(Scene.rgb565('#FFFFFF'),'#ffffff');
assert.deepEqual(Scene.dock,['dashboard','printer','files','camera','tools','console','drybox','settings']);
for(const page of [...Scene.dock,'network','telemetry']){
 const scene=Scene.render(p,page);assert(scene.startsWith('<svg '));assert(scene.endsWith('</svg>'));
 assert(!scene.includes('undefined'));assert(scene.includes('translate(24 80)'));assert(scene.includes('width="976" height="424"'));
 for(const title of Object.values(Scene.titles).slice(0,8))assert(scene.includes(title));
}
assert(svg.includes('x="340" y="176" width="360" height="12"'));
assert(svg.includes('x="736" y="0" width="1" height="330"'));
assert(svg.includes('font-size="96" font-weight="700"'));assert(svg.includes('Spool holder.gcode'));
assert(!svg.includes('Good afternoon'));assert(!svg.includes('p-grid'));
assert.equal(M.serialize(p),before,'render must not change exported theme');
const custom=M.copy(p);custom.palette.text='#123456';custom.metrics.radius_button=0;
assert(Scene.render(custom).includes(Scene.rgb565('#123456')));assert(Scene.render(custom).includes('rx="0"'));
assert(!Scene.render(p,'dashboard',{artwork:'https://example.invalid/a.png'}).includes('<image'));
assert(!Scene.render(p,'dashboard',{artwork:'data:image/png;base64," onload="bad'}).includes('<image'));
assert(Scene.render(p,'dashboard',{artwork:'data:image/png;base64,AAAA'}).includes('<image'));
const root=path.resolve(__dirname,'../..'),icons=fs.readFileSync(path.join(root,'main/ui_studio_icons.c'),'utf8');
for(const points of icons.matchAll(/PATH\(\w+,([^;]+)\);/g)){
 const d=[...points[1].matchAll(/\{(\d+),(\d+)\}/g)].map((m,i)=>(i?'L':'M')+m[1]+' '+m[2]).join(' ');if(d)assert(svg.includes('d="'+d+'"'),'native navigation icon changed');
}
console.log('PASS: Studio page scenes, native dock icons, Dashboard geometry, RGB565 colors, typography, palette/shape edits, artwork confinement and export immutability');

const components=Scene.renderComponents(p);assert(components.includes('Studio components'));assert(components.includes('Apply custom theme?'));assert(components.includes('opacity="0.4"'));assert(components.includes('rx="14"'));assert(components.includes('rx="22"'));assert(components.includes('font-weight="600"'));assert(Scene.renderComponents(custom).includes(Scene.rgb565('#123456')));assert(Scene.renderComponents(custom).includes('rx="0"'));assert.equal(M.serialize(p),before);
console.log('PASS: Studio control gallery uses native input/dialog recipes, disabled opacity, live palette/shape edits and immutable theme data');

const fake=(attributes,parentElement=null)=>({tagName:'rect',parentElement,getAttribute:key=>attributes[key]??null});
assert.deepEqual(Scene.colorRoles(fake({'data-theme-fill':'panel','data-theme-edge':'accent'}),p.palette).map(r=>r.key),['panel','accent']);
assert.deepEqual(Scene.colorRoles(fake({'data-theme-ink':'text'},fake({'data-theme-fill':'panel','data-theme-edge':'accent'})),p.palette).map(r=>r.key),['text','panel','accent']);
assert.deepEqual(Scene.colorRoles(fake({'data-theme-fill':'__proto__','data-theme-edge':'not-a-token'}),p.palette),[]);
for(const scene of [svg,components]){
 for(const match of scene.matchAll(/data-theme-(?:ink|fill|edge)="([^"]+)"/g))assert(Object.hasOwn(p.palette,match[1]));
 assert(scene.includes('data-theme-ink="text"'));assert(scene.includes('data-theme-fill="panel"'));assert(scene.includes('data-theme-edge="accent"'));
}
assert.equal(M.serialize(p),before);
console.log('PASS: pickable Studio token metadata, ancestor control roles, unknown-token confinement and unchanged firmware package data');
