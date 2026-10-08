import {el,button} from './api.js';
// Редактор говорит языком «Когда / И только если / Тогда» и переводит это в модель движка:
// «Когда» → triggers, «И только если» → conditions (дерево ВСЕ/ЛЮБОЕ), «Тогда» → actions.
const eventLabels={'contact.opened':'открылась','contact.closed':'закрылась','occupancy.detected':'появилось движение','occupancy.cleared':'движение пропало'};
const eventLeaf={'contact.opened':['contact',true],'contact.closed':['contact',false],'occupancy.detected':['occupancy',true],'occupancy.cleared':['occupancy',false]};
const capInfo={contact:{name:'Дверь или окно',icon:'door',states:[[true,'открыта'],[false,'закрыта']]},occupancy:{name:'Движение',icon:'motion',states:[[true,'есть движение'],[false,'нет движения']]},temperature:{name:'Температура',icon:'thermo'},battery:{name:'Заряд батареи',icon:'battery'}};
const ops=[['lt','меньше'],['le','не больше'],['eq','равно'],['ne','не равно'],['ge','не меньше'],['gt','больше']];
const clone=x=>JSON.parse(JSON.stringify(x));
const isGroup=n=>!!n&&(n.kind==='all'||n.kind==='any');
export function serializeConditions(tree){if(!tree)return [];const out=[];function visit(node,depth){if(depth>4||out.length>=16)throw new Error('Максимум 16 условий и 4 уровня вложенности');const index=out.length;out.push({});if(isGroup(node)){if(!node.children.length)throw new Error('Добавьте условие в набор');out[index]={kind:node.kind,children:node.children.map(c=>visit(c,depth+1))};}else out[index]=clone(node);return index;}visit(tree,1);return out;}
function inflate(nodes,index=0,depth=1){if(!nodes.length)return null;if(depth>4||!nodes[index])throw new Error('Некорректная структура условий');const n=clone(nodes[index]);if(isGroup(n))n.children=n.children.map(i=>inflate(nodes,i,depth+1));return n;}
export function validateDraft(rule,devices){if(!Number.isInteger(rule.id)||rule.id<1||rule.id>4294967295)return 'Некорректный ID';if(!rule.name.trim()||new TextEncoder().encode(rule.name).length>96)return 'Название: от 1 до 96 байт';if(!rule.triggers.length||rule.triggers.length>8)return 'Добавьте от 1 до 8 событий в «Когда»';for(const t of rule.triggers){const d=devices.find(d=>d.device_id===t.device_id);const cap=t.kind.startsWith('contact.')?'contact':'occupancy';if(!eventLabels[t.kind]||!d?.capabilities.includes(cap))return 'Устройство события удалено или не поддерживается';}
 const nodes=rule.conditions||[];if(nodes.length>16)return 'Максимум 16 условий';const seen=new Set();function walk(i,depth){if(depth>4||seen.has(i)||!nodes[i])throw Error('Некорректная вложенность условий');seen.add(i);const n=nodes[i];if(isGroup(n)){if(!n.children.length)throw Error('Пустой набор условий');for(const c of n.children)walk(c,depth+1);}else if(n.kind==='state'){const d=devices.find(d=>d.device_id===n.device_id);if(!d?.capabilities.includes(n.capability))throw Error('Устройство условия удалено');if(n.capability==='contact'||n.capability==='occupancy'){if(typeof n.value!=='boolean'||!['eq','ne'].includes(n.op))throw Error('Некорректное состояние');}else if(!Number.isSafeInteger(n.value)||!['eq','ne','lt','le','gt','ge'].includes(n.op))throw Error('Некорректное число');}else if(n.kind==='time_window'){if(!Number.isInteger(n.start)||!Number.isInteger(n.end)||n.start<0||n.end<0||n.start>=1440||n.end>=1440)throw Error('Проверьте время');}else throw Error('Неизвестное условие');}try{if(nodes.length)walk(0,1);if(seen.size!==nodes.length)throw Error('Некорректная структура');}catch(e){return e.message;}
 if(!rule.actions.length||rule.actions.length>8)return 'Добавьте от 1 до 8 действий в «Тогда»';for(const a of rule.actions){if(a.kind==='delay'){if(!Number.isInteger(a.delay_ms)||a.delay_ms<0||a.delay_ms>86400000)return 'Пауза: от 0 до 24 часов';}else if(a.kind==='set_channel_power'){const d=devices.find(d=>d.device_id===a.device_id);if(!d?.channels.some(c=>c.channel_id===a.channel_id)||typeof a.on!=='boolean')return 'Канал действия удалён или не найден';}else return 'Неизвестное действие';}return null;}

// Сценарии, сохранённые прошлой версией редактора, дублировали событие «Когда» условием
// того же состояния. При ВСЕ такое условие в момент события всегда истинно — убираем его.
function leafOf(t){const[capability,value]=eventLeaf[t.kind]||[];return {device_id:t.device_id,capability,value};}
const sameLeaf=(n,t)=>{const l=leafOf(t);return n.kind==='state'&&n.device_id===l.device_id&&n.capability===l.capability&&((n.op==='eq'&&n.value===l.value)||(n.op==='ne'&&n.value===!l.value));};
export function splitRule(rule){const triggers=clone(rule.triggers||[]);let tree=inflate(rule.conditions||[]);
 if(tree&&!isGroup(tree))tree={kind:'all',children:[tree]};if(!tree)tree={kind:'all',children:[]};
 if(tree.kind==='all')tree.children=tree.children.filter(c=>!triggers.some(t=>sameLeaf(c,t)));
 if(tree.children.length===1&&isGroup(tree.children[0]))tree=tree.children[0];
 const advanced=tree.kind==='any'&&tree.children.length>1||tree.children.some(isGroup);
 return {triggers,tree,advanced};}

const toTime=m=>`${String(Math.floor(m/60)).padStart(2,'0')}:${String(m%60).padStart(2,'0')}`;
const fromTime=v=>{const[h,m]=v.split(':').map(Number);return h*60+m;};
const fmtDelay=ms=>ms>=3600000&&ms%3600000===0?`${ms/3600000} ч`:ms>=60000&&ms%60000===0?`${ms/60000} мин`:`${ms/1000} с`;
export const channelName=(devices,a)=>{const d=devices.find(d=>d.device_id===a.device_id);return d?(d.channels.length>1?`${d.name}, канал ${a.channel_id}`:d.name):'удалённый канал';};
export const isTimed=(acts,i)=>{const a=acts[i],p=acts[i+1],b=acts[i+2];return a?.kind==='set_channel_power'&&a.on&&p?.kind==='delay'&&b?.kind==='set_channel_power'&&!b.on&&b.device_id===a.device_id&&b.channel_id===a.channel_id;};

// «Что произойдёт» — одной фразой обычным языком
function phrase(triggers,tree,actions,devices){const name=id=>devices.find(d=>d.device_id===id)?.name||'удалённое устройство';
 const ev={'contact.opened':n=>`откроется «${n}»`,'contact.closed':n=>`закроется «${n}»`,'occupancy.detected':n=>`«${n}» заметит движение`,'occupancy.cleared':n=>`у «${n}» пропадёт движение`};
 const t=triggers.map(x=>(ev[x.kind]||(n=>n))(name(x.device_id))).join(' или ');
 const cond=n=>{if(isGroup(n))return n.children.length>1?'('+n.children.map(cond).join(n.kind==='all'?' и ':' или ')+')':n.children.map(cond).join('');if(n.kind==='time_window')return `сейчас с ${toTime(n.start)} до ${toTime(n.end)}`;const info=capInfo[n.capability];if(info?.states){const v=n.op==='ne'?!n.value:n.value;return n.capability==='contact'?`«${name(n.device_id)}» ${v?'открыта':'закрыта'}`:`у «${name(n.device_id)}» ${v?'есть движение':'нет движения'}`;}return `${(info?.name||n.capability).toLowerCase()} «${name(n.device_id)}» ${ops.find(o=>o[0]===n.op)?.[1]||n.op} ${n.value}`;};
 const c=tree.children.map(cond).join(tree.kind==='all'?' и ':' или ');
 const parts=[];for(let i=0;i<actions.length;i++){const a=actions[i];if(isTimed(actions,i)){parts.push(`«${channelName(devices,a)}» включится на ${fmtDelay(actions[i+1].delay_ms)}`);i+=2;}else if(a.kind==='delay'){const nx=actions[i+1];if(nx&&nx.kind==='set_channel_power'){parts.push(`через ${fmtDelay(a.delay_ms)} «${channelName(devices,nx)}» ${nx.on?'включится':'выключится'}`);i++;}else parts.push(`пауза ${fmtDelay(a.delay_ms)}`);}else parts.push(`«${channelName(devices,a)}» ${a.on?'включится':'выключится'}`);}
 if(!t)return 'Выберите, что запускает сценарий, — блок «Когда».';
 const a=parts.join(', затем ')||'ничего не произойдёт';return `Когда ${t}${c?`, и если ${c}`:''}, ${a}.`;}
export function describeRule(rule,devices){const f=ruleToForm(rule);if(f)return formSummary(f,devices);try{const{triggers,tree}=splitRule(rule);return phrase(triggers,tree,rule.actions||[],devices);}catch{return '';}}

const NS='http://www.w3.org/2000/svg';
function icon(name){const s=document.createElementNS(NS,'svg');s.setAttribute('aria-hidden','true');const u=document.createElementNS(NS,'use');u.setAttribute('href','#i-'+name);s.append(u);return s;}
function select(items,value,change,label){const s=el('select',undefined,{'aria-label':label});for(const[v,text]of items)s.append(el('option',text,{value:String(v)}));if(!items.some(([v])=>String(v)===String(value)))s.append(el('option','Удалённое устройство',{value:String(value)}));s.value=String(value);s.addEventListener('change',()=>change(s.value));return s;}
function segmented(items,value,change,label){const box=el('div',undefined,{class:'segmented',role:'radiogroup','aria-label':label});for(const[v,text]of items){const b=button(text,()=>change(v),v===value?'seg on':'seg');b.setAttribute('role','radio');b.setAttribute('aria-checked',String(v===value));box.append(b);}return box;}
function iconButton(label,name,handler){const b=button('',handler,'row-remove');b.setAttribute('aria-label',label);b.append(icon(name));return b;}
function hintSeen(){try{return localStorage.getItem('zr.editorHint')==='1';}catch{return true;}}
function hideHint(){try{localStorage.setItem('zr.editorHint','1');}catch{}}

export const ScenarioEditor={mount(root,{devices,scenario,onSave,onDelete,templates=false}){
 const draft=clone(scenario);let{triggers,tree,advanced}=splitRule(draft);let saving=false,picker=null,actPicker=false,confirmDelete=false;
 const devName=id=>devices.find(d=>d.device_id===id)?.name||'удалённое устройство';
 const caps=['contact','occupancy','temperature','battery'].filter(c=>devices.some(d=>d.capabilities.includes(c)));
 const channelOptions=devices.flatMap(d=>d.channels.map(c=>[`${d.device_id}|${c.channel_id}`,d.channels.length>1?`${d.name} · канал ${c.channel_id}`:d.name]));
 const eventOptions=devices.flatMap(d=>d.capabilities.flatMap(cap=>Object.entries(eventLabels).filter(([kind])=>kind.startsWith(cap+'.')).map(([kind,label])=>[`${d.device_id}|${kind}`,`${d.name} — ${label}`])));
 const serializeSafe=()=>{try{return tree.children.length?serializeConditions(tree):[];}catch{return null;}};
 const snapshot=()=>JSON.stringify([draft.name,draft.enabled,triggers,serializeSafe(),draft.actions]);
 const autoName=()=>{const t=triggers[0];const act=draft.actions.find(a=>a.kind==='set_channel_power');return [t&&devName(t.device_id),act&&channelName(devices,act)].filter(Boolean).join(' → ').slice(0,90)||'Сценарий';};

 root.replaceChildren();const form=el('form',undefined,{class:'editor',novalidate:''});root.append(form);
 // Шапка: название (необязательно) и «Активен»
 const head=el('div',undefined,{class:'editor-head'});const name=el('input',undefined,{name:'name',maxlength:'96','aria-label':'Название сценария'});name.value=draft.name;name.addEventListener('input',()=>{draft.name=name.value;});
 const enabled=el('button','',{type:'button',role:'switch','aria-label':'Сценарий активен'});const paintEnabled=()=>{enabled.className=`switch ${draft.enabled?'on':''}`;enabled.setAttribute('aria-checked',String(draft.enabled));};paintEnabled();enabled.addEventListener('click',()=>{draft.enabled=!draft.enabled;paintEnabled();summary();});
 const nameCol=el('div',undefined,{class:'name-col'});nameCol.append(el('span','Название',{class:'field-label'}),name);head.append(nameCol,enabled);
 const tplBox=el('div',undefined,{class:'templates'});const hintBox=el('div',undefined,{class:'editor-hint'});
 const preview=el('div',undefined,{class:'editor-story'});const story=el('p','',{'aria-live':'polite'});preview.append(el('span','Что произойдёт',{class:'story-label'}),story);
 const whenBox=el('section',undefined,{class:'editor-section'}),ifBox=el('section',undefined,{class:'editor-section'}),thenBox=el('section',undefined,{class:'editor-section'});
 const error=el('p','',{class:'error',role:'alert'});const saveButton=el('button','Сохранить сценарий',{type:'submit',class:'primary'});const dangerBox=el('div',undefined,{class:'danger-zone'});
 form.append(tplBox,head,hintBox,preview,whenBox,ifBox,thenBox,error,saveButton,dangerBox);

 function summary(){name.placeholder=autoName();story.textContent=(draft.enabled?'':'Сценарий выключен. ')+phrase(triggers,tree,draft.actions,devices);}
 const sectionHead=(box,title,hint)=>{box.append(el('h3',title));if(hint)box.append(el('p',hint,{class:'muted'}));};
 const addButton=(label,handler,disabled)=>{const b=button('',handler,'add-row');b.append(icon('plus'),document.createTextNode(label));b.disabled=!!disabled;return b;};
 const redraw=()=>{error.textContent='';when();conditions();actions();summary();};

 function hint(){hintBox.replaceChildren();if(hintSeen()||!templates)return;hintBox.append(el('p','«Когда» — что запускает сценарий. «И только если» — что проверить в этот момент. «Тогда» — что сделать.'),button('Понятно',()=>{hideHint();hint();},'text-button'));}

 // Шаблоны — карточки, первый успешный сценарий в одно касание
 function templatesView(){tplBox.replaceChildren();if(!templates)return;const motion=devices.find(d=>d.capabilities.includes('occupancy')),door=devices.find(d=>d.capabilities.includes('contact'));const ch=channelOptions[0]?.[0].split('|');if(!ch)return;
  const lit=min=>[{kind:'set_channel_power',device_id:ch[0],channel_id:Number(ch[1]),on:true},{kind:'delay',delay_ms:min*60000},{kind:'set_channel_power',device_id:ch[0],channel_id:Number(ch[1]),on:false}];
  const list=[];
  if(motion)list.push(['motion','Свет при движении','Включить свет на минуту',[{device_id:motion.device_id,kind:'occupancy.detected'}],[],lit(1)]);
  if(door)list.push(['door','Свет при открытии двери','Включить свет на 3 минуты',[{device_id:door.device_id,kind:'contact.opened'}],[],lit(3)]);
  if(motion)list.push(['moon','Ночной свет','Движение с 23:00 до 07:00',[{device_id:motion.device_id,kind:'occupancy.detected'}],[{kind:'time_window',start:1380,end:420}],lit(1)]);
  if(!list.length)return;const grid=el('div',undefined,{class:'template-grid'});
  for(const[ic,title,sub,tr,cond,acts]of list){const b=button('',()=>{triggers=clone(tr);tree={kind:'all',children:clone(cond)};draft.actions=clone(acts);draft.name=title;name.value=title;templates=false;advanced=false;templatesView();hint();redraw();},'template-card');const mark=el('span',undefined,{class:'row-icon'});mark.append(icon(ic));const txt=el('span',undefined,{class:'template-text'});txt.append(el('strong',title),el('small',sub));b.append(mark,txt);grid.append(b);}
  tplBox.append(el('p','Начать с готового',{class:'row-text'}),grid,button('Создать с нуля',()=>{templates=false;templatesView();},'text-button'));}

 // Когда
 function when(){whenBox.replaceChildren();sectionHead(whenBox,'Когда',triggers.length>1?'Любое из этих событий запускает сценарий':null);
  triggers.forEach((t,i)=>{if(i)whenBox.append(el('div','или',{class:'joiner'}));const row=el('div',undefined,{class:'rule-row'});const ic=el('span',undefined,{class:'row-icon'});ic.append(icon(t.kind.startsWith('contact')?'door':'motion'));const fields=el('div',undefined,{class:'row-fields'});const cap=t.kind.split('.')[0];const own=devices.filter(d=>d.capabilities.includes('contact')||d.capabilities.includes('occupancy')).map(d=>[d.device_id,d.name]);fields.append(select(own,t.device_id,v=>{const d=devices.find(x=>x.device_id===v);t.device_id=v;if(d&&!d.capabilities.includes(cap))t.kind=d.capabilities.includes('contact')?'contact.opened':'occupancy.detected';redraw();},'Датчик'));const short={'contact.opened':'открылась','contact.closed':'закрылась','occupancy.detected':'появилось','occupancy.cleared':'пропало'};const kinds=Object.keys(short).filter(k=>k.startsWith(cap+'.')).map(k=>[k,short[k]]);fields.append(segmented(kinds,t.kind,v=>{t.kind=v;redraw();},'Событие'));row.append(ic,fields,iconButton('Удалить событие','x',()=>{triggers.splice(i,1);redraw();}));whenBox.append(row);});
  whenBox.append(addButton(triggers.length?'Ещё событие':'Событие',()=>{const[device_id,kind]=(eventOptions[0]?.[0]||'|contact.opened').split('|');triggers.push({device_id,kind});redraw();},!eventOptions.length||triggers.length>=8));}

 // И только если
 const countNodes=n=>isGroup(n)?1+n.children.reduce((s,c)=>s+countNodes(c),0):1;
 function newCondition(type,parentKind){if(type==='time')return {kind:'time_window',start:1380,end:420};if(type==='group')return {kind:parentKind==='all'?'any':'all',children:[newCondition(caps[0]||'time')]};const d=devices.find(d=>d.capabilities.includes(type));const binary=type==='contact'||type==='occupancy';return binary?{kind:'state',device_id:d.device_id,capability:type,op:'eq',value:type==='contact'?false:true}:{kind:'state',device_id:d.device_id,capability:type,op:'gt',value:type==='battery'?20:0};}
 function typePicker(group,depth){const box=el('div',undefined,{class:'type-picker'});box.append(el('p','Что проверить?',{class:'row-text'}));const grid=el('div',undefined,{class:'type-grid'});const items=[['time','Время суток','clock'],...caps.map(c=>[c,capInfo[c].name,capInfo[c].icon])];if(advanced&&depth<3)items.push(['group','Группа условий','flow']);
  for(const[type,label,ic]of items){const b=button('',()=>{const add=type==='group'?2:1;if(countNodes(tree)+add>16){error.textContent='Максимум 16 условий';return;}group.children.push(newCondition(type,group.kind));picker=null;redraw();},'type-card');b.append(icon(ic),el('span',label));grid.append(b);}
  box.append(grid,button('Отмена',()=>{picker=null;conditions();},'text-button'));return box;}
 function leafRow(n,parent,i){const row=el('div',undefined,{class:'rule-row'});const ic=el('span',undefined,{class:'row-icon'});const fields=el('div',undefined,{class:'row-fields'});
  if(n.kind==='time_window'){ic.append(icon('clock'));const line=el('div',undefined,{class:'time-line'});line.append(el('span','с',{class:'row-text'}));for(const key of ['start','end']){const input=el('input',undefined,{type:'time','aria-label':key==='start'?'Начало интервала':'Конец интервала'});input.value=toTime(n[key]);input.addEventListener('change',()=>{if(input.value){n[key]=fromTime(input.value);summary();}});line.append(input);if(key==='start')line.append(el('span','до',{class:'row-text'}));}fields.append(line);}
  else{const info=capInfo[n.capability]||{name:n.capability,icon:'flow'};ic.append(icon(info.icon));const own=devices.filter(d=>d.capabilities.includes(n.capability)).map(d=>[d.device_id,d.name]);fields.append(select(own,n.device_id,v=>{n.device_id=v;summary();},'Устройство условия'));
   if(info.states)fields.append(segmented(info.states,n.op==='ne'?!n.value:n.value,v=>{n.op='eq';n.value=v;redraw();},'Состояние'));
   else{const line=el('div',undefined,{class:'time-line'});line.append(select(ops,n.op,v=>{n.op=v;summary();},'Сравнение'));const value=el('input',undefined,{type:'number',inputmode:'numeric','aria-label':'Значение',class:'num'});value.value=n.value;value.addEventListener('input',()=>{n.value=Number(value.value);summary();});line.append(value);fields.append(line);}}
  row.append(ic,fields,iconButton('Удалить условие','x',()=>{parent.children.splice(i,1);redraw();}));return row;}
 function groupView(group,depth,parent,index){const box=el('div',undefined,{class:depth===1?'cond-root':'cond-group'});
  if(depth>1){const gh=el('div',undefined,{class:'group-head'});gh.append(el('span','Группа условий',{class:'row-text'}),segmented([['all','все должны выполняться'],['any','достаточно любого']],group.kind,v=>{group.kind=v;redraw();},'Логика группы'),iconButton('Удалить группу','x',()=>{parent.children.splice(index,1);redraw();}));box.append(gh);}
  else if(advanced&&group.children.length>1){const gh=el('div',undefined,{class:'group-head'});gh.append(segmented([['all','Все условия должны выполняться'],['any','Достаточно любого условия']],group.kind,v=>{group.kind=v;redraw();},'Логика условий'));box.append(gh);}
  group.children.forEach((c,i)=>{if(i)box.append(el('div',group.kind==='all'?'и':'или',{class:'joiner'}));box.append(isGroup(c)?groupView(c,depth+1,group,i):leafRow(c,group,i));});
  if(picker===group)box.append(typePicker(group,depth));else box.append(addButton(depth===1?'Условие':'Условие в группу',()=>{picker=group;conditions();},countNodes(tree)>=16));return box;}
 function conditions(){ifBox.replaceChildren();sectionHead(ifBox,'И только если',tree.children.length?null:'Необязательно. Например, только ночью');ifBox.append(groupView(tree,1,null,0));
  const simple=!tree.children.some(isGroup)&&!(tree.kind==='any'&&tree.children.length>1);
  const more=el('details',undefined,{class:'more'});if(advanced)more.open=true;more.append(el('summary','••• Дополнительно'));
  const adv=button(advanced?'Обычная логика условий':'Сложная логика условий',()=>{if(advanced&&!simple)return;advanced=!advanced;if(!advanced)tree.kind='all';conditions();summary();},'text-button');if(advanced&&!simple){adv.disabled=true;adv.textContent='В сценарии есть группы условий';}adv.setAttribute('aria-pressed',String(advanced));more.append(adv);ifBox.append(more);}

 // Тогда
 function channelSelect(a){return select(channelOptions,`${a.device_id}|${a.channel_id}`,v=>{const[id,c]=v.split('|');a.device_id=id;a.channel_id=Number(c);summary();},'Устройство');}
 function delayFields(target,read,write){const unitMin=read()>=60000&&read()%60000===0;const input=el('input',undefined,{type:'number',min:'0',step:'1',inputmode:'numeric','aria-label':'Длительность',class:'num'});input.value=unitMin?read()/60000:read()/1000;let unit;const apply=()=>{write(Math.round(Number(input.value)*(unit.value==='m'?60000:1000)));summary();};unit=select([['s','сек'],['m','мин']],unitMin?'m':'s',apply,'Единица времени');unit.classList.add('unit');input.addEventListener('input',apply);target.append(input,unit);}
 function actions(){thenBox.replaceChildren();sectionHead(thenBox,'Тогда',null);let step=0;const acts=draft.actions;
  for(let i=0;i<acts.length;i++){const a=acts[i];const start=i;const row=el('div',undefined,{class:'rule-row'});row.append(el('span',String(++step),{class:'step-number'}));const fields=el('div',undefined,{class:'row-fields'});let span=1;
   if(isTimed(acts,i)){span=3;const l1=el('div',undefined,{class:'time-line'});l1.append(el('span','Включить на время',{class:'row-text strong'}),channelSelect(a));fields.append(l1);const line=el('div',undefined,{class:'time-line'});line.append(el('span','на',{class:'row-text'}));delayFields(line,()=>acts[start+1].delay_ms,v=>{acts[start+1].delay_ms=v;});fields.append(line);const sync=fields.querySelector('select');sync.addEventListener('change',()=>{acts[start+2].device_id=a.device_id;acts[start+2].channel_id=a.channel_id;});}
   else if(a.kind==='delay'){const line=el('div',undefined,{class:'time-line'});line.append(el('span','Подождать',{class:'row-text'}));delayFields(line,()=>a.delay_ms,v=>{a.delay_ms=v;});fields.append(line);}
   else fields.append(segmented([[true,'Включить'],[false,'Выключить']],a.on,v=>{a.on=v;actions();summary();},'Действие'),channelSelect(a));
   row.append(fields);const tools=el('div',undefined,{class:'row-tools vertical'});const n=span;tools.append(iconButton('Удалить действие','x',()=>{acts.splice(start,n);actions();summary();}));if(start)tools.append(iconButton('Переместить выше','up',()=>{const ps=start>=3&&isTimed(acts,start-3)?3:1;const moved=acts.splice(start,n);acts.splice(start-ps,0,...moved);actions();summary();}));row.append(tools);thenBox.append(row);i+=span-1;}
  const[d0,c0]=(channelOptions[0]?.[0]||'|1').split('|');const full=acts.length;const ch=()=>({device_id:d0,channel_id:Number(c0)});
  if(actPicker){const box=el('div',undefined,{class:'type-picker'});box.append(el('p','Что сделать?',{class:'row-text'}));const grid=el('div',undefined,{class:'type-grid'});
   const items=[['Включить на время','clock',()=>[{kind:'set_channel_power',...ch(),on:true},{kind:'delay',delay_ms:60000},{kind:'set_channel_power',...ch(),on:false}],full>5||!channelOptions.length],['Включить','power',()=>[{kind:'set_channel_power',...ch(),on:true}],full>=8||!channelOptions.length],['Выключить','power',()=>[{kind:'set_channel_power',...ch(),on:false}],full>=8||!channelOptions.length],['Пауза','pause',()=>[{kind:'delay',delay_ms:60000}],full>=8]];
   for(const[label,ic,make,off]of items){const b=button('',()=>{acts.push(...make());actPicker=false;actions();summary();},'type-card');b.disabled=off;b.append(icon(ic),el('span',label));grid.append(b);}
   box.append(grid,button('Отмена',()=>{actPicker=false;actions();},'text-button'));thenBox.append(box);}
  else thenBox.append(addButton('Действие',()=>{actPicker=true;actions();},full>=8));}

 function danger(){dangerBox.replaceChildren();if(!onDelete)return;if(!confirmDelete){dangerBox.append(button('Удалить сценарий',()=>{confirmDelete=true;danger();},'danger-button'));return;}
  dangerBox.append(el('span','Удалить без возможности восстановить?',{class:'row-text'}),button('Удалить',()=>onDelete(),'danger-button solid'),button('Нет',()=>{confirmDelete=false;danger();},'text-button'));}

 const editor={serialize(){if(!draft.name.trim()){draft.name=autoName();name.value=draft.name;}return {...clone(draft),triggers:clone(triggers),conditions:tree.children.length?serializeConditions(tree):[]};},
  isDirty(){return snapshot()!==base;},
  async save(){if(saving)return;let value;try{value=editor.serialize();const invalid=validateDraft(value,devices);if(invalid)throw Error(invalid);}catch(e){error.textContent=e.message;return;}saving=true;saveButton.disabled=true;error.textContent='';try{await onSave(value);base=snapshot();}catch(e){error.textContent=e.message;}finally{saving=false;saveButton.disabled=false;}}};
 form.addEventListener('submit',e=>{e.preventDefault();editor.save();});templatesView();hint();redraw();danger();let base=snapshot();return editor;}};

// ===== Конфигуратор в духе Livicom: один экран «Условия запуска → Время работы → Действия → Таймер» =====
// Условие = датчик + событие. «И»: сценарий запускается, когда выполнены все условия; «ИЛИ» — любое.
// В модель движка это ложится так: каждое условие даёт событие запуска (triggers) и проверку
// состояния (conditions, группа ВСЕ/ЛЮБОЕ); время работы — time_window; таймер — пауза
// и обратные команды тем же каналам. Модель движка не меняется.
const condEvents={contact:[[true,'Открытие','contact.opened'],[false,'Закрытие','contact.closed']],occupancy:[[true,'Обнаружено движение','occupancy.detected'],[false,'Движение прекратилось','occupancy.cleared']]};
const condLabel=c=>condEvents[c.capability]?.find(e=>e[0]===c.value)?.[1]||'';
const condKind=c=>condEvents[c.capability]?.find(e=>e[0]===c.value)?.[2];
const timerChips=[[30000,'30 с'],[60000,'1 мин'],[300000,'5 мин'],[900000,'15 мин'],[3600000,'1 ч']];
const fitBytes=(t,max=96)=>{const enc=new TextEncoder();while(enc.encode(t).length>max)t=t.slice(0,-1);return t;};
export const FORM_LIMITS={conds:8};

export function formToRule(f,id){
 const leaves=f.conds.map(c=>({kind:'state',device_id:c.device_id,capability:c.capability,op:'eq',value:c.value}));
 const triggers=[];for(const c of f.conds){const t={device_id:c.device_id,kind:condKind(c)};if(!triggers.some(x=>x.device_id===t.device_id&&x.kind===t.kind))triggers.push(t);}
 const logic=leaves.length>1?f.logic:'all';let tree=null;
 if(leaves.length){if(f.window){const tw={kind:'time_window',start:f.window.start,end:f.window.end};tree=logic==='all'?{kind:'all',children:[...leaves,tw]}:{kind:'all',children:[{kind:'any',children:leaves},tw]};}else tree={kind:logic,children:leaves};}
 const set=(a,on)=>({kind:'set_channel_power',device_id:a.device_id,channel_id:a.channel_id,on});
 const actions=f.acts.map(a=>set(a,a.on));if(f.timer&&f.acts.length)actions.push({kind:'delay',delay_ms:f.timer},...f.acts.map(a=>set(a,!a.on)));
 return {id,name:f.name.trim(),enabled:f.enabled,triggers,conditions:tree?serializeConditions(tree):[],actions};}

// Обратно: сохранённый сценарий → форма. null — сценарий сложнее формы (открывается расширенный редактор).
export function ruleToForm(rule){try{
 const f={name:rule.name||'',enabled:rule.enabled!==false,logic:'all',conds:[],window:null,acts:[],timer:null};
 let tree=inflate(rule.conditions||[]);const leafOk=n=>n&&n.kind==='state'&&condEvents[n.capability]&&typeof n.value==='boolean'&&(n.op==='eq'||n.op==='ne');
 const toCond=n=>({device_id:n.device_id,capability:n.capability,value:n.op==='ne'?!n.value:n.value});
 let leaves=[];
 if(tree){if(!isGroup(tree))tree={kind:'all',children:[tree]};let kids=tree.children;
  const tw=kids.filter(k=>k.kind==='time_window');if(tw.length>1)return null;
  if(tw.length){if(tree.kind!=='all')return null;f.window={start:tw[0].start,end:tw[0].end};kids=kids.filter(k=>k.kind!=='time_window');
   if(kids.length===1&&isGroup(kids[0])){if(!kids[0].children.every(leafOk))return null;f.logic=kids[0].kind;leaves=kids[0].children;}else{if(!kids.every(leafOk))return null;leaves=kids;}}
  else{if(!kids.every(leafOk))return null;f.logic=tree.kind;leaves=kids;}}
 f.conds=leaves.map(toCond);
 const trig=rule.triggers||[];
 if(!f.conds.length){for(const t of trig){const ev=Object.entries(condEvents).flatMap(([cap,list])=>list.map(e=>({cap,e}))).find(x=>x.e[2]===t.kind);if(!ev)return null;f.conds.push({device_id:t.device_id,capability:ev.cap,value:ev.e[0]});}f.logic=f.conds.length>1?'any':'all';}
 else{const want=new Set(f.conds.map(c=>`${c.device_id}|${condKind(c)}`));const have=new Set(trig.map(t=>`${t.device_id}|${t.kind}`));if(want.size!==have.size||[...want].some(k=>!have.has(k)))return null;}
 if(!f.conds.length||f.conds.length>FORM_LIMITS.conds)return null;
 const acts=rule.actions||[];const di=acts.findIndex(a=>a.kind==='delay');
 const sets=di<0?acts:acts.slice(0,di);if(!sets.length||!sets.every(a=>a.kind==='set_channel_power'))return null;
 f.acts=sets.map(a=>({device_id:a.device_id,channel_id:a.channel_id,on:a.on}));
 if(di>=0){const back=acts.slice(di+1);if(back.length!==sets.length||!back.every((b,i)=>b.kind==='set_channel_power'&&b.device_id===sets[i].device_id&&b.channel_id===sets[i].channel_id&&b.on===!sets[i].on))return null;f.timer=acts[di].delay_ms;if(!f.timer)return null;}
 return f;}catch{return null;}}

export function formSummary(f,devices){const name=id=>devices.find(d=>d.device_id===id)?.name||'удалённое устройство';
 const ev=c=>c.capability==='contact'?`${c.value?'откроется':'закроется'} «${name(c.device_id)}»`:`«${name(c.device_id)}» ${c.value?'заметит движение':'перестанет видеть движение'}`;
 if(!f.conds.length)return 'Добавьте условие запуска.';
 const when=f.conds.map(ev).join(f.logic==='any'?' или ':' и ');
 const win=f.window?`, с ${toTime(f.window.start)} до ${toTime(f.window.end)}`:'';
 if(!f.acts.length)return `Когда ${when}${win} — добавьте действие.`;
 const what=f.acts.map(a=>`«${channelName(devices,a)}» ${a.on?'включится':'выключится'}`).join(', ');
 const back=f.timer?`, через ${fmtDelay(f.timer)} — обратно`:'';
 return `Когда ${when}${win}, ${what}${back}.`;}

export function formAutoName(f,devices){const d=id=>devices.find(x=>x.device_id===id);const c=f.conds[0],a=f.acts[0];if(!c||!a)return 'Сценарий';
 const relay=d(a.device_id);const target=relay?(relay.channels.length>1?`канал ${a.channel_id}`:relay.name):'канал';
 return fitBytes(`${d(c.device_id)?.name||'Датчик'}: ${condLabel(c).toLowerCase()} → ${a.on?'вкл.':'выкл.'} ${target}`);}

export function formValidate(f){if(!f.conds.length)return 'Добавьте хотя бы одно условие запуска';if(!f.acts.length)return 'Добавьте хотя бы одно действие';
 const total=f.acts.length*(f.timer?2:1)+(f.timer?1:0);if(total>8)return f.timer?'С таймером — не больше 3 устройств в действиях':'Не больше 8 действий';
 if(f.timer&&(f.timer<1000||f.timer>86400000))return 'Таймер: от 1 секунды до 24 часов';return null;}

export const ScenarioForm={mount(root,{devices,scenario=null,nextId,onSave,onDelete,onTest,setTitle=()=>{},setBack=()=>{},setDone=()=>{}}){
 const existing=!!scenario;const id=scenario?.id??nextId;
 let f=scenario?ruleToForm(scenario):null;
 const blank=()=>({name:'',enabled:true,logic:'all',conds:[],window:null,acts:[],timer:null});
 let view=f?'form':'ideas',base=JSON.stringify(f||blank()),saving=false,testing=false,editIndex=-1,pickDevice=null,actDraft=null,confirmDel=false;if(!f)f=blank();
 const sensors=devices.filter(d=>d.capabilities.some(c=>condEvents[c]));
 const channels=devices.flatMap(d=>d.channels.map(c=>({device_id:d.device_id,channel_id:c.channel_id,title:d.channels.length>1?`Канал ${c.channel_id}`:d.name,sub:d.channels.length>1?d.name:'Выключатель'})));
 const devName=id=>devices.find(d=>d.device_id===id)?.name||'Удалённое устройство';
 const capOf=d=>d.capabilities.find(c=>condEvents[c]);
 const go=v=>{view=v;render();root.scrollTop=0;};
 const section=(title,extra)=>{const s=el('section',undefined,{class:'lv-section'});const h=el('div',undefined,{class:'lv-head'});h.append(el('h3',title));if(extra)h.append(extra);s.append(h);return s;};
 const list=()=>el('div',undefined,{class:'lv-list'});
 const addButton=(text,fn,disabled)=>{const b=button('',fn,'lv-add');b.append(icon('plus'),document.createTextNode(text));b.disabled=!!disabled;return b;};
 const rowIcon=name=>{const m=el('span',undefined,{class:'row-icon'});m.append(icon(name));return m;};
 const textCol=(title,sub)=>{const t=el('span',undefined,{class:'lv-text'});t.append(el('strong',title));if(sub)t.append(el('small',sub));return t;};
 const toggle=(on,label,fn)=>{const b=el('button','',{type:'button',role:'switch','aria-label':label,'aria-checked':String(on),class:`switch ${on?'on':''}`});b.addEventListener('click',fn);return b;};

 function render(){root.replaceChildren();setDone(null);
  if(view==='ideas'){setTitle('Новый сценарий');setBack(null);return ideas();}
  if(view==='form'){setTitle(existing?'Сценарий':'Новый сценарий');setBack(existing?null:()=>go('ideas'));return main();}
  setBack(()=>go(view==='cond-event'?'cond-device':'form'));
  ({'cond-device':condDevice,'cond-event':condEvent,'act-pick':actPick})[view]();}

 // Популярные сценарии — как «идеи» при создании нового сценария
 function ideas(){const box=el('div',undefined,{class:'lv-screen'});box.append(el('p','Начните с готовой идеи или создайте сценарий с нуля. Сценарий запускается по событию от датчика.',{class:'muted lv-lead'}));
  const motion=sensors.find(d=>d.capabilities.includes('occupancy')),door=sensors.find(d=>d.capabilities.includes('contact'));const one=(cap)=>{const l=sensors.filter(d=>d.capabilities.includes(cap));return l.length===1?l[0]:null;};const ch=channels.length===1?channels[0]:null;
  const idea=(ic,title,sub,make)=>{const b=button('',()=>{f={...blank(),...make()};go('form');},'lv-idea');b.append(rowIcon(ic),textCol(title,sub));return b;};
  const l=list();
  const cond=(cap,value)=>{const d=one(cap);return d?[{device_id:d.device_id,capability:cap,value}]:[];};const act=on=>ch?[{device_id:ch.device_id,channel_id:ch.channel_id,on}]:[];
  if(motion)l.append(idea('motion','Свет при движении','Движение → включить, через 1 мин выключить',()=>({name:'Свет при движении',conds:cond('occupancy',true),acts:act(true),timer:60000})));
  if(door)l.append(idea('door','Свет при открытии двери','Открытие → включить на 3 мин',()=>({name:'Свет при открытии двери',conds:cond('contact',true),acts:act(true),timer:180000})));
  if(motion)l.append(idea('moon','Ночная подсветка','Движение с 23:00 до 07:00 → свет на 1 мин',()=>({name:'Ночная подсветка',conds:cond('occupancy',true),window:{start:1380,end:420},acts:act(true),timer:60000})));
  if(motion)l.append(idea('power','Выключить, когда никого нет','Движение прекратилось → выключить',()=>({name:'Выключить, когда никого нет',conds:cond('occupancy',false),acts:act(false)})));
  box.append(section('Популярные сценарии'),l);
  const own=list();const b=button('',()=>{f=blank();go('form');},'lv-idea');b.append(rowIcon('plus'),textCol('Создать с нуля','Условия, время работы, действия'));own.append(b);box.append(own);
  if(!sensors.length||!channels.length)box.append(el('p','Для сценария нужны датчик и управляемое устройство. Подключите их на вкладке «Дом».',{class:'muted'}));
  root.append(box);}

 function main(){const box=el('div',undefined,{class:'lv-screen'});
  // Название и активность
  const top=list();const nameRow=el('label',undefined,{class:'lv-row lv-name'});const input=el('input',undefined,{name:'name',maxlength:'96',placeholder:formAutoName(f,devices),'aria-label':'Название сценария'});input.value=f.name;input.addEventListener('input',()=>{f.name=input.value;});nameRow.append(el('span','Название',{class:'lv-label'}),input);
  const act=el('div',undefined,{class:'lv-row'});act.append(textCol('Сценарий активен',f.enabled?null:'Не будет запускаться, пока выключен'),toggle(f.enabled,'Сценарий активен',()=>{f.enabled=!f.enabled;render();}));top.append(nameRow,act);box.append(top);
  // Условия запуска
  const logic=f.conds.length>1?segmented([['all','И'],['any','ИЛИ']],f.logic,v=>{f.logic=v;render();},'Логика условий'):null;
  const cs=section('Условия запуска',logic);if(f.conds.length>1)cs.append(el('p',f.logic==='all'?'Запустится, когда выполнены все условия':'Запустится, когда выполнено любое из условий',{class:'lv-note'}));
  const cl=list();f.conds.forEach((c,i)=>{const row=el('div',undefined,{class:'lv-row'});const open=button('',()=>{editIndex=i;pickDevice=devices.find(d=>d.device_id===c.device_id)||null;go(pickDevice?'cond-event':'cond-device');},'lv-open');open.append(rowIcon(c.capability==='contact'?'door':'motion'),textCol(devName(c.device_id),condLabel(c)));open.setAttribute('aria-label',`Условие: ${devName(c.device_id)} — ${condLabel(c)}. Изменить`);
   row.append(open,iconButton(`Удалить условие «${devName(c.device_id)}»`,'x',()=>{f.conds.splice(i,1);render();}));cl.append(row);});
  cl.append(addButton('Условие',()=>{editIndex=-1;pickDevice=null;go('cond-device');},f.conds.length>=FORM_LIMITS.conds||!sensors.length));cs.append(cl);box.append(cs);
  // Время работы
  const ts=section('Время работы');const tl=list();const all=el('div',undefined,{class:'lv-row'});all.append(textCol('Круглосуточно',f.window?null:'Сценарий работает в любое время'),toggle(!f.window,'Круглосуточно',()=>{f.window=f.window?null:{start:1380,end:420};render();}));tl.append(all);
  if(f.window)for(const[key,label]of [['start','С'],['end','До']]){const r=el('label',undefined,{class:'lv-row'});const t=el('input',undefined,{type:'time','aria-label':label});t.value=toTime(f.window[key]);t.addEventListener('change',()=>{if(t.value){f.window[key]=fromTime(t.value);paintSummary();}});r.append(el('span',label,{class:'lv-label'}),t);tl.append(r);}
  ts.append(tl);box.append(ts);
  // Действия
  const as=section('Действия');const al=list();f.acts.forEach((a,i)=>{const row=el('div',undefined,{class:'lv-row'});const ch=channels.find(c=>c.device_id===a.device_id&&c.channel_id===a.channel_id);const title=ch?.title||channelName(devices,a);
   const seg=segmented([[true,'Вкл'],[false,'Выкл']],a.on,v=>{a.on=v;render();},`Действие для «${ch?channelName(devices,a):title}»`);row.append(rowIcon('switch'),textCol(title,ch?.sub),seg,iconButton(`Удалить действие «${ch?channelName(devices,a):title}»`,'x',()=>{f.acts.splice(i,1);render();}));al.append(row);});
  al.append(addButton('Действия',()=>{actDraft=f.acts.map(a=>`${a.device_id}|${a.channel_id}`);go('act-pick');},!channels.length));as.append(al);box.append(as);
  // Таймер
  const tms=section('Таймер');const tml=list();const tr=el('div',undefined,{class:'lv-row'});tr.append(textCol('Таймер',f.timer?`Через ${fmtDelay(f.timer)} устройства переключатся обратно`:'Вернуть устройства в обратное состояние через заданное время'),toggle(!!f.timer,'Таймер',()=>{f.timer=f.timer?null:60000;render();}));tml.append(tr);
  if(f.timer){const chips=el('div',undefined,{class:'lv-chips'});for(const[ms,label]of timerChips){const b=button(label,()=>{f.timer=ms;render();},'chip'+(f.timer===ms?' on':''));b.setAttribute('aria-pressed',String(f.timer===ms));chips.append(b);}
   const other=el('div',undefined,{class:'lv-row lv-custom'});const unit=f.timer%3600000===0?3600000:f.timer%60000===0?60000:1000;const n=el('input',undefined,{type:'number',min:'1',inputmode:'numeric','aria-label':'Длительность таймера'});n.value=String(f.timer/unit);
   const u=select([['1000','сек'],['60000','мин'],['3600000','ч']],String(unit),()=>apply(),'Единица таймера');const apply=()=>{const v=Math.round(Number(n.value)*Number(u.value));if(v>=1000&&v<=86400000){f.timer=v;paintSummary();chips.querySelectorAll('.chip').forEach((c,i)=>{c.classList.toggle('on',timerChips[i][0]===v);c.setAttribute('aria-pressed',String(timerChips[i][0]===v));});}};n.addEventListener('change',apply);
   other.append(el('span','Своё время',{class:'lv-label'}),n,u);const wrap=el('div',undefined,{class:'lv-row lv-wrap'});wrap.append(chips);tml.append(wrap,other);}
  tms.append(tml);box.append(tms);
  // Итог, проверка, сохранение
  const story=el('div',undefined,{class:'editor-story'});const p=el('p','',{'aria-live':'polite'});story.append(el('span','Что произойдёт',{class:'story-label'}),p);box.append(story);
  const err=el('p','',{class:'error',role:'alert'});const status=el('p','',{class:'muted test-status','aria-live':'polite'});
  const saveBtn=button('Сохранить сценарий',save,'primary wide');const testBtn=button('',async()=>{if(testing||!onTest||!f.acts.length)return;testing=true;testBtn.disabled=true;try{await onTest(f.acts.map(a=>({kind:'set_channel_power',...a})),t=>{status.textContent=t;});}catch(e){status.textContent=e.message;}finally{testing=false;testBtn.disabled=!f.acts.length;}},'secondary wide');testBtn.append(icon('play'),document.createTextNode('Проверить действия'));testBtn.disabled=!f.acts.length;
  box.append(err,saveBtn,testBtn,status);
  if(existing&&onDelete){const del=button('Удалить сценарий',()=>{confirmDel=true;render();},'danger-button');if(confirmDel){const row=el('div',undefined,{class:'button-row'});row.append(el('span','Сценарий удалится без возможности восстановления — кроме кнопки «Отменить» в уведомлении.',{class:'row-text strong'}),button('Удалить',onDelete,'danger-button solid'),button('Отмена',()=>{confirmDel=false;render();},'text-button'));box.append(row);}else box.append(del);}
  function paintSummary(){p.textContent=formSummary(f,devices);const bad=formValidate(f);saveBtn.disabled=!!bad;saveBtn.title=bad||'';setDone(bad?null:save);input.placeholder=formAutoName(f,devices);}
  async function save(){const bad=formValidate(f);if(bad){err.textContent=bad;return;}if(saving)return;const value=formToRule({...f,name:f.name.trim()||formAutoName(f,devices)},id);const v2=validateDraft(value,devices);if(v2){err.textContent=v2;return;}saving=true;saveBtn.disabled=true;try{await onSave(value);base=JSON.stringify(f);}catch(e){err.textContent=e.message;}finally{saving=false;saveBtn.disabled=false;}}
  root.append(box);paintSummary();}

 function condDevice(){setTitle('Условие');const box=el('div',undefined,{class:'lv-screen'});box.append(el('h2','Выберите устройство',{class:'lv-q'}));const l=list();
  for(const d of sensors){const cap=capOf(d);const b=button('',()=>{pickDevice=d;go('cond-event');},'lv-idea');b.append(rowIcon(cap==='contact'?'door':'motion'),textCol(d.name,cap==='contact'?'Датчик открытия':'Датчик движения'));l.append(b);}box.append(l);root.append(box);}
 function condEvent(){setTitle('Условие');const d=pickDevice;const cap=capOf(d);const box=el('div',undefined,{class:'lv-screen'});box.append(el('h2','Выберите событие',{class:'lv-q'}),el('p',d.name,{class:'muted lv-lead'}));const l=list();const cur=editIndex>=0?f.conds[editIndex]:null;
  for(const[value,label]of condEvents[cap]){const sel=cur&&cur.device_id===d.device_id&&cur.value===value;const b=button('',()=>{const c={device_id:d.device_id,capability:cap,value};if(f.conds.some((x,i)=>i!==editIndex&&x.device_id===c.device_id&&x.value===c.value)){go('form');return;}if(editIndex>=0)f.conds[editIndex]=c;else f.conds.push(c);go('form');},'lv-idea'+(sel?' selected':''));b.setAttribute('aria-pressed',String(!!sel));b.append(textCol(label,null));if(sel){const m=el('span',undefined,{class:'lv-check'});m.append(icon('check'));b.append(m);}l.append(b);}
  box.append(l);root.append(box);}
 function actPick(){setTitle('Действия');const box=el('div',undefined,{class:'lv-screen'});box.append(el('h2','Выберите устройства',{class:'lv-q'}),el('p','Отметьте, чем управлять. Включить или выключить — выберете на следующем шаге.',{class:'muted lv-lead'}));const l=list();
  for(const c of channels){const key=`${c.device_id}|${c.channel_id}`;const on=actDraft.includes(key);const b=button('',()=>{actDraft=on?actDraft.filter(k=>k!==key):[...actDraft,key];render();},'lv-idea'+(on?' selected':''));b.setAttribute('role','checkbox');b.setAttribute('aria-checked',String(on));b.append(rowIcon('switch'),textCol(c.title,c.sub));const m=el('span',undefined,{class:'lv-check'+(on?'':' empty')});if(on)m.append(icon('check'));b.append(m);l.append(b);}
  box.append(l,button('Готово',()=>{const keep=f.acts.filter(a=>actDraft.includes(`${a.device_id}|${a.channel_id}`));for(const k of actDraft)if(!keep.some(a=>`${a.device_id}|${a.channel_id}`===k)){const[device_id,ch]=k.split('|');keep.push({device_id,channel_id:Number(ch),on:true});}f.acts=keep;go('form');},'primary wide'));root.append(box);}

 render();
 return {isDirty:()=>view!=='ideas'&&JSON.stringify(f)!==base,form:()=>clone(f)};}};
