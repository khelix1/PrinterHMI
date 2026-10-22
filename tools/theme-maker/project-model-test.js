'use strict';
const assert=require('node:assert/strict'),M=require('./theme-model'),P=require('./project-model');
for(const base of ['studio','operator','foundry','glass']){
 const theme=M.create(base),before=M.serialize(theme),text=P.serialize(theme,{...P.defaults,view:'components',tab:'layout',snap:true,size:16});
 const opened=P.parse(text);assert.deepEqual(opened.theme,theme);assert.equal(opened.editor.snap,true);assert.equal(opened.editor.view,'components');assert.equal(M.serialize(theme),before);
}
const p=P.create(M.create('studio'));
for(const change of [v=>v.version=2,v=>v.extra=true,v=>v.editor.snap=1,v=>v.editor.size=3,v=>v.editor.view='script',v=>v.theme.palette.text='bad',v=>v.editor.extra=true,v=>delete v.editor.grid]){const bad=M.copy(p);change(bad);assert.throws(()=>P.parse(JSON.stringify(bad)));}
assert.throws(()=>P.parse('x'.repeat(P.maxBytes+1)));assert.throws(()=>P.parse('[]'));assert.throws(()=>P.parse('{}'));assert.throws(()=>P.parse('{broken'));
const artwork=M.copy(p);artwork.theme.schema_version=3;artwork.theme.background={asset:'bg-'+'a'.repeat(64)+'.phmibg',fit:'cover',position_x:38,position_y:62,overlay_color:'#121212',overlay_opacity:42};assert.deepEqual(P.parse(JSON.stringify(artwork)),artwork);
console.log('PASS: editable projects round-trip all renderer bases and workspace settings, preserve artwork metadata, reject invalid versions/fields/themes/states, and leave firmware exports unchanged');
