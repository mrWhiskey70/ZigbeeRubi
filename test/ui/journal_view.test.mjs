import test from 'node:test';import assert from 'node:assert/strict';
import {buildJournal} from '../../components/web_ui/assets/journal_view.js';
const devices=[{device_id:'0000000000000003',name:'Люстра',capabilities:[],channels:[{channel_id:1}]}];
const rules=[{id:1,name:'Свет при движении',actions:[{kind:'set_channel_power',device_id:devices[0].device_id,channel_id:1,on:true},{kind:'delay',delay_ms:60000},{kind:'set_channel_power',device_id:devices[0].device_id,channel_id:1,on:false}]}];
const e=(sequence,reason,extra={})=>({sequence,monotonic_ms:sequence*1000,rule_id:1,device_id:devices[0].device_id,channel_id:1,status:'confirmed',reason,...extra});
test('sent and confirmed merge into one readable row per command',()=>{const rows=buildJournal([e(1,'trigger'),e(2,'sent',{status:'pending'}),e(3,'confirmed'),e(4,'sent',{status:'pending'}),e(5,'confirmed')],rules,devices);
 assert.deepEqual(rows.map(r=>r.title+' — '+r.sub),['Люстра — Выключен сценарием «Свет при движении»','Люстра — Включён сценарием «Свет при движении»','«Свет при движении» — Сработал']);assert.ok(rows.every(r=>r.tone!=='bad'));});
test('timeout is shown as a failure on the command row',()=>{const rows=buildJournal([e(1,'trigger'),e(2,'sent',{status:'pending'}),e(3,'timeout',{status:'timeout'})],rules,devices);assert.equal(rows[0].tone,'bad');assert.equal(rows.length,2);});
