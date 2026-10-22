/* Editable desktop projects are separate from firmware theme packages. */
(function(g){
'use strict';
const M=typeof module!=='undefined'?require('./theme-model'):g.ThemeModel;
const format='printerhmi.theme-maker.project',maxBytes=65536;
const pages=['dashboard','printer','files','camera','tools','console','drybox','settings','network','telemetry'];
const defaults={page:'dashboard',tab:'design',view:'device',region:'active_print',grid:true,snap:false,size:8};
function editor(value,theme){
 if(!value||typeof value!=='object'||Array.isArray(value)||Object.keys(value).some(k=>!Object.hasOwn(defaults,k))||Object.keys(value).length!==Object.keys(defaults).length)throw Error('Invalid project workspace.');
 if(!pages.includes(value.page)||!['design','layout','project'].includes(value.tab)||!['device','components','json'].includes(value.view)||typeof value.region!=='string'||value.region.length>64||typeof value.grid!=='boolean'||typeof value.snap!=='boolean'||![4,8,16,32].includes(value.size))throw Error('Unsupported project workspace settings.');
 if(theme.base_theme!=='studio'&&['camera','tools','console'].includes(value.page))throw Error('This page requires Studio.');
 return {...value};
}
function create(theme,state=defaults){const clean=M.parse(M.serialize(theme));return {format,version:1,theme:clean,editor:editor(state,clean)};}
function parse(text){
 if(typeof text!=='string'||new TextEncoder().encode(text).length>maxBytes)throw Error('Project metadata exceeds 64 KiB.');
 const p=JSON.parse(text);
 if(!p||Array.isArray(p)||Object.keys(p).sort().join(',')!=='editor,format,theme,version'||p.format!==format||p.version!==1)throw Error('Unsupported Theme Maker project.');
 return create(p.theme,p.editor);
}
function serialize(theme,state){const text=JSON.stringify(create(theme,state),null,2)+'\n';if(new TextEncoder().encode(text).length>maxBytes)throw Error('Project metadata exceeds 64 KiB.');return text;}
const api={create,parse,serialize,defaults,maxBytes};if(typeof module!=='undefined')module.exports=api;else g.ThemeProject=api;
})(typeof globalThis!=='undefined'?globalThis:this);
