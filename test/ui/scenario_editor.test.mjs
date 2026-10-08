import test from 'node:test';import assert from 'node:assert/strict';import {JSDOM} from 'jsdom';
import {ScenarioEditor,ScenarioForm,formToRule,ruleToForm,formValidate,formAutoName,serializeConditions,validateDraft,splitRule,describeRule} from '../../components/web_ui/assets/scenario_editor.js';
const devices=[{device_id:'0000000000000001',name:'Дверь',capabilities:['contact'],channels:[]},{device_id:'0000000000000002',name:'PIR',capabilities:['occupancy'],channels:[]},{device_id:'0000000000000003',name:'Реле',capabilities:[],channels:[{channel_id:1},{channel_id:2},{channel_id:3}]}];
const state=(id,cap)=>({kind:'state',device_id:id,capability:cap,op:'eq',value:true});
const tree={kind:'all',children:[{kind:'any',children:[state(devices[0].device_id,'contact'),state(devices[1].device_id,'occupancy')]},{kind:'time_window',start:1380,end:120}]};
const rule={id:1,name:'<img onerror=alert(1)>',enabled:true,triggers:[{device_id:devices[1].device_id,kind:'occupancy.detected'}],conditions:[],actions:[{kind:'set_channel_power',device_id:devices[2].device_id,channel_id:1,on:true},{kind:'delay',delay_ms:60000},{kind:'set_channel_power',device_id:devices[2].device_id,channel_id:1,on:false}]};
test('nested ALL ANY converts to bounded codec indices',()=>{const nodes=serializeConditions(tree);assert.equal(nodes.length,5);assert.deepEqual(nodes[0].children,[1,4]);assert.deepEqual(nodes[1].children,[2,3]);assert.equal(validateDraft({...rule,conditions:nodes},devices),null);});
test('invalid references, empty groups, triggers and depth are rejected',()=>{assert.ok(validateDraft({...rule,triggers:[]},devices));assert.ok(validateDraft({...rule,conditions:[{kind:'all',children:[]}]},devices));assert.ok(validateDraft({...rule,actions:[{...rule.actions[0],channel_id:4}]},devices));let deep=state(devices[0].device_id,'contact');for(let i=0;i<4;i++)deep={kind:'all',children:[deep]};assert.throws(()=>serializeConditions(deep));});
test('name is text; failed save retains draft; double click sends once',async()=>{const dom=new JSDOM('<div id="root"></div>');globalThis.document=dom.window.document;const root=document.getElementById('root');let calls=0,reject;const editor=ScenarioEditor.mount(root,{devices,scenario:rule,onSave:()=>{calls++;return new Promise((_,r)=>{reject=r;});}});assert.equal(root.querySelector('input[name="name"]').value,rule.name);assert.equal(root.querySelectorAll('img').length,0);const first=editor.save();await editor.save();assert.equal(calls,1);reject(new Error('Нет связи'));await first;assert.equal(editor.serialize().name,rule.name);assert.match(root.textContent,/Нет связи/);delete globalThis.document;});
test('legacy rules: condition duplicating the trigger is dropped only under ALL',()=>{const d=devices;const leaf={kind:'state',device_id:d[1].device_id,capability:'occupancy',op:'eq',value:true};
 const all=splitRule({triggers:[{device_id:d[1].device_id,kind:'occupancy.detected'}],conditions:serializeConditions({kind:'all',children:[leaf,{kind:'time_window',start:0,end:60}]})});
 assert.deepEqual(all.tree.children.map(c=>c.kind),['time_window']);assert.equal(all.advanced,false);
 const any=splitRule({triggers:[{device_id:d[1].device_id,kind:'occupancy.detected'}],conditions:serializeConditions({kind:'any',children:[leaf,{kind:'time_window',start:0,end:60}]})});
 assert.equal(any.tree.children.length,2);assert.equal(any.advanced,true);});
test('When / If / Then saves triggers and conditions separately and reads as a sentence',()=>{const dom=new JSDOM('<div id="root"></div>');globalThis.document=dom.window.document;const editor=ScenarioEditor.mount(document.getElementById('root'),{devices,scenario:rule,onSave:async()=>{}});const v=editor.serialize();assert.deepEqual(v.triggers,rule.triggers);assert.deepEqual(v.conditions,[]);assert.equal(validateDraft(v,devices),null);
 assert.match(describeRule(rule,devices),/^Когда «PIR» заметит движение, «Реле, канал 1» включится, через 1 мин — обратно\.$/);delete globalThis.document;});

const D=devices.map(d=>d.device_id);
test('form: И / ИЛИ, time and timer map onto triggers / conditions / actions and back',()=>{
 const f={name:'',enabled:true,logic:'any',conds:[{device_id:D[0],capability:'contact',value:true},{device_id:D[1],capability:'occupancy',value:true}],window:{start:1380,end:420},acts:[{device_id:D[2],channel_id:1,on:true},{device_id:D[2],channel_id:2,on:false}],timer:300000};
 const r=formToRule({...f,name:'x'},5);assert.equal(validateDraft(r,devices),null);
 assert.deepEqual(r.triggers.map(t=>t.kind),['contact.opened','occupancy.detected']);
 assert.deepEqual(r.conditions.map(n=>n.kind),['all','any','state','state','time_window']);
 assert.deepEqual(r.actions.map(a=>a.kind==='delay'?a.delay_ms:a.on),[true,false,300000,false,true]);
 assert.deepEqual(ruleToForm(r),{...f,name:'x'});
 const and=formToRule({...f,name:'x',logic:'all',window:null,timer:null},6);assert.deepEqual(and.conditions.map(n=>n.kind),['all','state','state']);assert.deepEqual(ruleToForm(and).logic,'all');});
test('form: legacy and wizard rules open in the form, complex ones fall back to the advanced editor',()=>{
 assert.equal(ruleToForm(rule).timer,60000);
 const wiz={...rule,triggers:[{device_id:D[0],kind:'contact.opened'}],conditions:[{kind:'time_window',start:0,end:60}],actions:[rule.actions[0]]};const f=ruleToForm(wiz);assert.deepEqual(f.conds,[{device_id:D[0],capability:'contact',value:true}]);assert.deepEqual(f.window,{start:0,end:60});
 assert.equal(ruleToForm({...rule,conditions:serializeConditions(tree)}),null);
 assert.equal(ruleToForm({...rule,actions:[{kind:'delay',delay_ms:1000},rule.actions[0]]}),null);});
test('form: limits and byte-safe auto name; new scenario has nothing preselected',async()=>{
 const base={name:'',enabled:true,logic:'all',conds:[],window:null,acts:[],timer:null};assert.ok(formValidate(base));
 const four=[1,2,3,1].map((c,i)=>({device_id:D[2],channel_id:c,on:i%2===0}));assert.match(formValidate({...base,conds:[{device_id:D[1],capability:'occupancy',value:true}],acts:four,timer:1000}),/3 устройств/);
 const long=[{...devices[1],name:'Очень длинное название датчика'.repeat(5)},devices[2]];assert.ok(new TextEncoder().encode(formAutoName({...base,conds:[{device_id:D[1],capability:'occupancy',value:true}],acts:[{device_id:D[2],channel_id:1,on:true}]},long)).length<=96);
 const dom=new JSDOM('<div id="root"></div>');globalThis.document=dom.window.document;const root=document.getElementById('root');let saved=null;
 const ui=ScenarioForm.mount(root,{devices,nextId:9,onSave:async v=>{saved=v;}});
 [...root.querySelectorAll('.lv-idea')].find(b=>b.textContent.includes('Создать с нуля')).click();
 assert.equal(root.querySelectorAll('.lv-list .lv-open').length,0);assert.ok(root.querySelector('button.primary.wide').disabled);assert.equal(ui.isDirty(),false);
 const click=t=>[...root.querySelectorAll('button')].find(b=>b.textContent.includes(t)).click();
 click('Условие');click('PIR');click('Обнаружено движение');click('Действия');click('Канал 2');click('Готово');
 assert.equal(ui.isDirty(),true);root.querySelector('button.primary.wide').click();await new Promise(r=>setTimeout(r,0));
 assert.equal(saved.id,9);assert.equal(saved.name,'PIR: обнаружено движение → вкл. канал 2');assert.deepEqual(saved.actions,[{kind:'set_channel_power',device_id:D[2],channel_id:2,on:true}]);delete globalThis.document;});
