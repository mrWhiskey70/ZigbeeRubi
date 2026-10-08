import {Api,el,button,icon} from './api.js';
import {ScenarioEditor,ScenarioForm,ruleToForm,describeRule,channelName,isTimed} from './scenario_editor.js';
import {renderDevices} from './devices_view.js';
import {renderJournal} from './journal_view.js';
import {mountSimulator} from './simulator_panel.js';
const $=id=>document.getElementById(id);
let devices=[],rules=[],system={},entries=[],tab='devices',simMounted=false,editor=null,lastSnapshot='',hubAt={mono:0,wall:0},lastOk=0;
const pending={};
const labels={devices:'Дом',scenarios:'Сценарии',journal:'События',settings:'Настройки'};
const hubNow=()=>hubAt.mono+(Date.now()-hubAt.wall);
const plural=(n,a,b,c)=>{const m=n%10,h=n%100;return m===1&&h!==11?a:m>=2&&m<=4&&(h<12||h>14)?b:c;};
const ago=ms=>{const s=Math.round(ms/1000);if(s<60)return 'только что';const m=Math.round(s/60);if(m<60)return `${m} мин назад`;const h=Math.round(m/60);return `${h} ч назад`;};
const left=ms=>{const s=Math.max(0,Math.ceil(ms/1000));return s>=60?`${Math.floor(s/60)} мин ${String(s%60).padStart(2,'0')} с`:`${s} с`;};

function notify(text){$('notice').textContent=text;$('notice').hidden=false;}
let toastTimer;
function toast(text,undo){const t=$('toast');const hide=()=>{t.hidden=true;};t.replaceChildren(el('span',text));if(undo)t.append(button('Отменить',()=>{hide();undo();},'text-button'));const x=button('',hide,'toast-close');x.setAttribute('aria-label','Закрыть уведомление');x.append(icon('x'));t.append(x);t.hidden=false;const arm=()=>{clearTimeout(toastTimer);toastTimer=setTimeout(hide,undo?10000:4000);};t.onmouseenter=t.onfocusin=()=>clearTimeout(toastTimer);t.onmouseleave=t.onfocusout=arm;arm();}
async function run(fn){try{$('notice').hidden=true;await fn();await refresh(true);}catch(e){notify(e.message);}}
function sheet(title,build){const d=el('dialog',undefined,{class:'mini-sheet'});const close=()=>{d.close();d.remove();};d.addEventListener('close',()=>d.remove());d.append(el('h2',title));build(d,close);document.body.append(d);d.showModal();return close;}

function navigate(name){tab=name;for(const key of Object.keys(labels))$('view-'+key).hidden=key!==name;document.querySelectorAll('[data-tab]').forEach(b=>{b.classList.toggle('active',b.dataset.tab===name);if(b.dataset.tab===name)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current');});$('compact-bar').textContent=labels[name];if(name==='settings')renderSettings();window.scrollTo(0,0);}
for(const b of document.querySelectorAll('[data-tab]'))b.addEventListener('click',()=>navigate(b.dataset.tab));

// Ожидающие действия сценария: последний запуск + паузы в его действиях, по часам хаба
function ruleTimer(r){if(!system.pending)return null;const last=[...entries].reverse().find(e=>e.rule_id===r.id&&e.reason==='trigger');if(!last)return null;
 if(entries.some(e=>e.reason==='manual_override'&&e.monotonic_ms>=last.monotonic_ms&&r.actions.some(a=>a.device_id===e.device_id&&a.channel_id===e.channel_id)))return null;
 let t=0;const acts=r.actions;for(let i=0;i<acts.length;i++){if(acts[i].kind==='delay'){t+=acts[i].delay_ms;const next=acts.slice(i+1).find(a=>a.kind==='set_channel_power');const due=last.monotonic_ms+t;if(next&&due>hubNow())return {due,verb:next.on?'включит':'выключит',next,text:`${next.on?'Включит':'Выключит'} «${channelName(devices,next)}»`};}}return null;}
function lastRun(r){const last=[...entries].reverse().find(e=>e.rule_id===r.id&&(e.reason==='trigger'||e.reason==='conditions_not_true'));if(!last)return null;return last.reason==='trigger'?`Сработал ${ago(hubNow()-last.monotonic_ms)}`:`Не сработал ${ago(hubNow()-last.monotonic_ms)}: условия не выполнены`;}

// Дом: спокойная строка «Всё в порядке», карточки появляются только из-за событий
function renderStatus(){const root=$('status');root.replaceChildren();const cards=[];
 const offline=devices.filter(d=>!d.available);if(offline.length)cards.push({tone:'warn',icon:'signal',title:offline.length===1?`«${offline[0].name}» не отвечает`:`${offline.length} ${plural(offline.length,'устройство','устройства','устройств')} не отвечают`,sub:'Проверьте питание и расстояние до хаба',action:['Проверить',()=>deviceSheet(offline[0])]});
 const open=devices.filter(d=>d.available&&d.capabilities.includes('contact')&&d.states.contact===true);for(const d of open)cards.push({tone:'warn',icon:'door',title:`«${d.name}» открыта`,sub:'Дверь или окно открыто',action:['Подробнее',()=>deviceSheet(d)]});
 const timed=new Set();for(const r of rules){const tm=ruleTimer(r);if(tm)timed.add(`${tm.next.device_id}|${tm.next.channel_id}`);if(tm)cards.push({tone:'info',icon:'clock',title:tm.text,sub:`Сценарий «${r.name}»`,due:tm.due});}
 const on=devices.flatMap(d=>d.channels.filter(c=>c.power===true&&!timed.has(`${d.device_id}|${c.channel_id}`)).map(c=>({d,c})));if(on.length)cards.push({tone:'info',icon:'power',title:on.length===1?`Включено: ${channelName(devices,{device_id:on[0].d.device_id,channel_id:on[0].c.channel_id})}`:`Включено каналов: ${on.length}`,action:[on.length===1?'Выключить':'Выключить все',()=>run(async()=>{for(const x of on)await Api.request('/channels/power',{method:'POST',body:{device_id:x.d.device_id,channel_id:x.c.channel_id,on:false}});toast(on.length===1?'Выключено':'Всё выключено');})]});
 if(!cards.length){const ok=el('p',undefined,{class:'all-good'});ok.append(icon('check'),el('span','Всё в порядке'));root.append(ok);return;}
 for(const c of cards){const card=el('div',undefined,{class:`status-card tone-${c.tone}`});const mark=el('span',undefined,{class:'row-icon'});mark.append(icon(c.icon));const txt=el('div',undefined,{class:'status-text'});const title=el('strong',c.title);txt.append(title);const sub=el('small',c.sub||'');if(c.due){sub.dataset.due=String(c.due);sub.dataset.prefix='через ';sub.textContent=`через ${left(c.due-hubNow())} · ${c.sub}`;sub.dataset.suffix=` · ${c.sub}`;}if(c.sub||c.due)txt.append(sub);card.append(mark,txt);if(c.action)card.append(button(c.action[0],c.action[1],'secondary small'));root.append(card);}}

async function command(d,c){const key=`${d.device_id}|${c.channel_id}`;pending[key]=true;renderDeviceCards();try{const result=await Api.request('/channels/power',{method:'POST',body:{device_id:d.device_id,channel_id:c.channel_id,on:c.power!==true}});for(let i=0;i<12;i++){const op=await Api.request('/operations/'+result.operation_id);if(op.status==='confirmed')break;if(['failed','timeout'].includes(op.status))throw Error('Команда не подтверждена устройством');if(i===11)throw Error('Устройство ещё не подтвердило состояние');await new Promise(resolve=>setTimeout(resolve,450));}}catch(e){notify(e.message);}finally{delete pending[key];await refresh(true);}}

function deviceSheet(d){sheet(d.name,(box,close)=>{const input=el('input',undefined,{maxlength:'96','aria-label':'Название устройства'});input.value=d.name;const form=el('form',undefined,{class:'sheet-form'});form.append(el('label','Название',{class:'field-label'}),input);
 const info=el('dl',undefined,{class:'facts'});for(const[k,v]of [['Тип',d.channels.length?'Выключатель':d.capabilities.includes('contact')?'Датчик открытия':'Датчик движения'],['Связь',d.available?'В сети':'Не отвечает']]){const r=el('div');r.append(el('dt',k),el('dd',v));info.append(r);}
 const tech=el('details',undefined,{class:'tech'});tech.append(el('summary','Технические сведения'));const tl=el('dl',undefined,{class:'facts'});const r=el('div');r.append(el('dt','ID'),el('dd',d.device_id));tl.append(r);tech.append(tl);
 const save=el('button','Сохранить',{type:'submit',class:'primary'});form.append(save,info,tech);
 form.addEventListener('submit',e=>{e.preventDefault();const name=input.value.trim();if(!name)return;close();run(async()=>{await Api.request('/devices/'+d.device_id,{method:'PUT',body:{name}});toast('Название сохранено');});});
 const confirmRow=el('div',undefined,{class:'button-row'});const del=button('Удалить из хаба',()=>{del.replaceWith(confirmRow);},'danger-button');confirmRow.append(el('span','Устройство придётся подключать заново.',{class:'row-text'}),button('Удалить',()=>{close();run(async()=>{await Api.request('/devices/'+d.device_id,{method:'DELETE'});toast(`«${d.name}» удалено`);});},'danger-button solid'));
 box.append(form,del,button('Закрыть',close,'text-button'));setTimeout(()=>input.focus(),50);});}
function renderDeviceCards(){renderDevices($('devices'),devices,{command,pending,open:deviceSheet,join});}

// Сценарии
// Создание — пошаговый мастер без заранее выбранного железа; сохранённый сценарий открывается в редакторе.
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const setBack=fn=>{const b=$('editor-back');b.hidden=!fn;b.onclick=fn||null;$('close-editor').hidden=!!fn;};
const setDone=fn=>{const b=$('editor-done');b.hidden=!fn;b.onclick=fn||null;};
const setTitle=(t,quiet=false)=>{const h=$('editor-title');h.textContent=t;h.classList.toggle('visually-hidden',quiet);};
async function testActions(actions,say){const seen=new Map();for(const a of actions)if(a.kind==='set_channel_power'&&!seen.has(`${a.device_id}|${a.channel_id}`))seen.set(`${a.device_id}|${a.channel_id}`,a);
 const list=[...seen.values()].map(a=>{const d=devices.find(x=>x.device_id===a.device_id);const c=d?.channels.find(x=>x.channel_id===a.channel_id);return {a,before:c?.power};});if(!list.length)return;
 const send=(a,on)=>Api.request('/channels/power',{method:'POST',body:{device_id:a.device_id,channel_id:a.channel_id,on}});
 const names=list.map(({a})=>channelName(devices,a)).join(', ');const on=list[0].a.on;
 const same=list.every(({a})=>a.on===on);say(same?`«${names}» ${on?'включится':'выключится'} на 5 секунд…`:'Выполняю действия на 5 секунд…');for(const{a}of list)await send(a,a.on);
 await sleep(5000);for(const{a,before}of list)await send(a,typeof before==='boolean'?before:!a.on);say('Проверка закончена — всё вернулось как было.');refresh(true);}
async function createScenario(value){await Api.request('/scenarios',{method:'POST',body:value});editor=null;$('editor-dialog').close();toast('Сценарий создан');await refresh(true);}
async function updateScenario(value){await Api.request('/scenarios/'+value.id,{method:'PUT',body:value});editor=null;$('editor-dialog').close();toast('Сценарий сохранён');await refresh(true);}
function deleteScenario(existing){return async()=>{const copy=JSON.parse(JSON.stringify(existing));editor=null;$('editor-dialog').close();await run(async()=>{await Api.request('/scenarios/'+existing.id,{method:'DELETE'});toast(`«${existing.name}» удалён`,()=>run(()=>Api.request('/scenarios',{method:'POST',body:copy})));});};}
// Новый и сохранённый сценарий открываются в одном конфигураторе «Условия → Время работы → Действия → Таймер».
// Сценарий сложнее формы (вложенные группы, паузы между действиями) — в расширенном редакторе.
function editScenario(existing){setBack(null);setDone(null);setTitle(existing?'Сценарий':'Новый сценарий');const root=$('editor-root');
 if(existing&&!ruleToForm(existing))editor=ScenarioEditor.mount(root,{devices,scenario:existing,onSave:updateScenario,onDelete:deleteScenario(existing)});
 else editor=ScenarioForm.mount(root,{devices,scenario:existing||null,nextId:Math.max(0,...rules.map(r=>r.id))+1,onSave:existing?updateScenario:createScenario,onDelete:existing?deleteScenario(existing):null,onTest:testActions,setTitle,setBack,setDone});
 $('editor-dialog').showModal();root.scrollTop=0;}
function closeEditor(){if(editor?.isDirty()){sheet('Отменить изменения?',(box,close)=>{box.append(el('p','Изменения в сценарии не сохранятся.',{class:'muted'}));const row=el('div',undefined,{class:'sheet-actions'});row.append(button('Продолжить редактирование',close,'secondary'),button('Отменить изменения',()=>{close();editor=null;$('editor-dialog').close();},'danger-button solid'));box.append(row);});return;}editor=null;setBack(null);setDone(null);$('editor-dialog').close();}
$('new-rule').addEventListener('click',()=>editScenario());
$('close-editor').addEventListener('click',closeEditor);
$('editor-dialog').addEventListener('cancel',e=>{e.preventDefault();closeEditor();});

function renderRules(){const root=$('scenarios');root.replaceChildren();
 if(!rules.length){const empty=el('div',undefined,{class:'empty-state'});const mark=el('div',undefined,{class:'empty-icon'});mark.append(icon('flow'));empty.append(mark,el('h2','Создайте первый сценарий'),el('p','Например: появилось движение — включить свет на минуту.',{class:'muted'}),button('Создать сценарий',()=>editScenario(),'primary'));root.append(empty);return;}
 // Сначала то, что сейчас ждёт таймера: видно, что локальная автоматика работает
 const timers=rules.map(r=>({r,tm:r.enabled?ruleTimer(r):null}));
 for(const{r,tm}of timers.sort((a,b)=>(b.tm?1:0)-(a.tm?1:0))){const card=el('article',undefined,{class:`rule-card ${r.enabled?'':'off'} ${tm?'running':''}`});
  const open=el('button',undefined,{type:'button',class:'rule-open','aria-label':`Открыть сценарий «${r.name}»`});const ri=el('span',undefined,{class:'rule-icon'});ri.append(icon('flow'));const content=el('span',undefined,{class:'rule-content'});content.append(el('span',r.name,{class:'rule-title'}),el('span',describeRule(r,devices),{class:'rule-desc'}));
  if(tm){const s=el('span','',{class:'rule-live'});s.dataset.due=String(tm.due);s.dataset.prefix=`Сработал · ${tm.verb} через `;s.dataset.suffix='';s.textContent=s.dataset.prefix+left(tm.due-hubNow());content.append(s);}
  else{const lr=r.enabled?lastRun(r):'Выключен';if(lr)content.append(el('span',lr,{class:'rule-last'}));}
  open.append(ri,content);open.addEventListener('click',()=>editScenario(r));
  const toggle=button('',()=>run(async()=>{await Api.request('/scenarios/'+r.id,{method:'PUT',body:{...r,enabled:!r.enabled}});toast(r.enabled?'Сценарий выключен':'Сценарий включён');}),`switch ${r.enabled?'on':''}`);toggle.setAttribute('role','switch');toggle.setAttribute('aria-label',`Сценарий ${r.name}`);toggle.setAttribute('aria-checked',String(r.enabled));
  card.append(open,toggle);root.append(card);}}

// Настройки: пользовательское отдельно, инженерное — в «Режиме разработчика»
function renderSettings(){const root=$('settings');root.replaceChildren();const card=el('div',undefined,{class:'settings-card'});
 const form=el('form',undefined,{class:'editor-section'});form.append(el('h2','Часовой пояс'));const select=el('select',undefined,{'aria-label':'Часовой пояс'});for(let i=-12;i<=14;++i)select.append(el('option',`UTC${i>=0?'+':''}${i}${i===7?' · Томск':''}`,{value:i*60}));if(!Array.from(select.options).some(o=>Number(o.value)===system.offset_minutes))select.append(el('option',String(system.offset_minutes)+' мин',{value:system.offset_minutes}));select.value=system.offset_minutes??420;const save=el('button','Сохранить',{type:'submit',class:'secondary'});const row=el('div',undefined,{class:'edit-row'});row.append(select,save);form.append(row,el('p','Нужен для условий по времени суток.',{class:'muted'}));form.addEventListener('submit',e=>{e.preventDefault();run(async()=>{await Api.request('/settings',{method:'PUT',body:{offset_minutes:Number(select.value)}});toast('Часовой пояс сохранён');});});card.append(form);
 if(system.mode==='target'){const wifi=el('form',undefined,{class:'editor-section'});wifi.append(el('h2','Wi-Fi'));const ssid=el('input',undefined,{placeholder:'Имя сети (SSID)',maxlength:32,required:'',autocomplete:'off','aria-label':'Имя Wi-Fi сети'});const password=el('input',undefined,{type:'password',placeholder:'Пароль сети',maxlength:64,autocomplete:'new-password','aria-label':'Пароль Wi-Fi'});const connect=el('button','Подключить и сохранить',{type:'submit',class:'primary'});wifi.append(ssid,password,connect,el('p','После подключения откройте хаб в домашней сети: http://zigbeerubi.local. Если адрес не открывается, найдите IP в роутере.',{class:'muted'}));wifi.addEventListener('submit',async e=>{e.preventDefault();connect.disabled=true;const secret=password.value;password.value='';try{const res=await fetch('/api/network/connect',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:ssid.value,password:secret,save_credentials:true})});if(!res.ok)throw new Error('Не удалось подключиться');notify('Подключение запрошено. Перейдите в домашнюю Wi-Fi сеть.');}catch(error){notify(error.message);}finally{connect.disabled=false;}});card.append(wifi);}
 const devs=el('div',undefined,{class:'editor-section'});devs.append(el('h2','Устройства'));const add=button('Добавить устройство',join,'secondary');devs.append(el('p',`${devices.length} ${plural(devices.length,'устройство','устройства','устройств')} в хабе`,{class:'muted'}),add);card.append(devs);
 const about=el('div',undefined,{class:'editor-section'});about.append(el('h2','О системе'));for(const[label,value]of [['Хаб',system.mode==='simulator'?'Симулятор на компьютере':'ZigbeeRubi на ESP32-C6'],['Версия',system.version||'—'],['Работа без интернета','Да, вся логика на хабе']]){const r=el('div',undefined,{class:'settings-row'});r.append(el('span',label),el('strong',value));about.append(r);}card.append(about);
 root.append(card);
 const info=$('dev-info');info.replaceChildren();for(const[label,value]of [['Режим',system.mode],['Часы хаба',system.time_known?'Синхронизированы':'Ожидаем синхронизацию'],['Время работы',left(hubNow()).replace(' с',' с')],['Ожидающие действия',String(system.pending||0)],['Проверка на оборудовании',system.hardware_verified?'Выполнена':'Предстоит']]){const r=el('div',undefined,{class:'settings-row'});r.append(el('span',label),el('strong',value));info.append(r);}
 info.append(el('p','Сценарий запускается событием из «Когда»; условия «И только если» проверяются в этот момент. Первый отчёт датчика после включения хаба только задаёт состояние. Нет данных от датчика — условие не выполнено.',{class:'muted'}));
 if(system.mode==='simulator')$('dev').open=true;}

$('refresh-log').addEventListener('click',()=>run(async()=>{}));
function join(){run(async()=>{await Api.request('/network/join',{method:'POST',body:{seconds:60}});sheet('Подключение открыто на 60 секунд',(box,close)=>{box.append(el('p','Переведите датчик в режим сопряжения по его инструкции — обычно это долгое нажатие кнопки на корпусе.',{class:'muted'}));if(system.mode==='simulator'){const row=el('div',undefined,{class:'sheet-actions'});for(const[label,kind]of [['Виртуальная дверь','door'],['Виртуальный датчик движения','motion']])row.append(button(label,()=>{close();run(async()=>{await Api.request('/sim/pair',{method:'POST',body:{kind}});toast('Устройство добавлено');});},'secondary'));box.append(row);}box.append(button('Закрыть',close,'text-button'));});});}
$('join').addEventListener('click',join);

async function refresh(force){try{const[dev,scene,sys,log]=await Promise.all([Api.request('/devices'),Api.request('/scenarios'),Api.request('/system'),Api.request('/log')]);
  hubAt={mono:sys.monotonic_ms??0,wall:Date.now()};lastOk=Date.now();
  $('connection').hidden=true;$('offline').hidden=true;
  const {monotonic_ms,...stable}=sys;const snap=JSON.stringify([dev,scene,stable,log]);if(!force&&snap===lastSnapshot)return;lastSnapshot=snap;
  devices=dev.devices;rules=scene.scenarios;system=sys;entries=log.entries;
  renderStatus();renderDeviceCards();renderRules();renderJournal($('journal'),entries,rules,devices,hubNow());
  const sim=system.mode==='simulator';$('sim-panel').hidden=!sim;if(sim&&!simMounted){mountSimulator($('sim-panel'),()=>refresh(true),notify,()=>devices);simMounted=true;}
  if(tab==='settings'&&!$('view-settings').contains(document.activeElement))renderSettings();}
 catch(e){$('connection').hidden=false;$('connection').textContent='Хаб недоступен';$('offline').hidden=false;$('offline-time').textContent=lastOk?`Последние данные: ${new Date(lastOk).toLocaleTimeString('ru-RU',{hour:'2-digit',minute:'2-digit'})}`:'Данных ещё не было';}}
// Обратный отсчёт таймеров без перерисовки экрана
setInterval(()=>{let expired=false;for(const s of document.querySelectorAll('[data-due]')){const ms=Number(s.dataset.due)-hubNow();if(ms<=0&&!s.dataset.fired){s.dataset.fired='1';expired=true;}s.textContent=(s.dataset.prefix||'')+left(ms)+(s.dataset.suffix||'');}if(expired)refresh(true);},1000);
// Liquid Glass: компактное название раздела появляется после прокрутки большого заголовка
addEventListener('scroll',()=>{document.body.classList.toggle('scrolled',scrollY>56);},{passive:true});
await refresh(true);setInterval(()=>refresh(false),2000);
