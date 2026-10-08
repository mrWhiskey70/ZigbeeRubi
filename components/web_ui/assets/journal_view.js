import {el} from './api.js';
// «События»: лента по дням. Одна команда — одна строка, видно что произошло и почему:
// сценарием, вручную или по датчику. Время по часам телефона пересчитывается из монотонного
// времени хаба: момент события = сейчас − (часы хаба сейчас − часы хаба в момент события).
const NS='http://www.w3.org/2000/svg';
function icon(name){const s=document.createElementNS(NS,'svg');s.setAttribute('aria-hidden','true');const u=document.createElementNS(NS,'use');u.setAttribute('href','#i-'+name);s.append(u);return s;}
export function buildJournal(entries,rules=[],devices=[]){const ruleName=id=>rules.find(r=>r.id===id)?.name;const dev=id=>devices.find(d=>d.device_id===id);
 const target=l=>{const d=dev(l.device_id);if(!d)return l.channel_id?`Канал ${l.channel_id}`:'Устройство';return d.channels.length>1?`${d.name}, канал ${l.channel_id}`:d.name;};
 const step={},rows=[];const sorted=[...entries].sort((a,b)=>a.sequence-b.sequence);const merged=new Set();
 for(let i=0;i<sorted.length;i++){const l=sorted[i];if(merged.has(l.sequence))continue;const who=ruleName(l.rule_id);const by=who?`сценарием «${who}»`:'сценарием';
  if(l.reason==='trigger'){step[l.rule_id]=0;rows.push({ms:l.monotonic_ms,title:who?`«${who}»`:'Сценарий',sub:'Сработал',source:'rule',tone:'info'});continue;}
  if(l.reason==='sent'){const rule=rules.find(r=>r.id===l.rule_id);const k=step[l.rule_id]??0;step[l.rule_id]=k+1;const act=rule?.actions.filter(a=>a.kind==='set_channel_power')[k];const known=act&&act.device_id===l.device_id&&act.channel_id===l.channel_id;
   const next=sorted.slice(i+1).find(n=>n.rule_id===l.rule_id&&n.device_id===l.device_id&&n.channel_id===l.channel_id&&['confirmed','failed','timeout'].includes(n.reason));if(next)merged.add(next.sequence);
   const ok=next?.reason==='confirmed',failed=next&&!ok;
   rows.push({ms:l.monotonic_ms,title:target(l),sub:failed?`Не ответило на команду ${by}`:`${known?(act.on?'Включён':'Выключен'):'Переключён'} ${by}${ok?'':' · ждём подтверждения'}`,source:'rule',tone:failed?'bad':ok?'ok':'wait'});continue;}
  const map={confirmed:[target(l),'Состояние подтверждено','device','ok'],conditions_not_true:[who?`«${who}»`:'Сценарий','Не сработал: условия не выполнены','rule','muted'],manual_override:[target(l),'Переключён вручную · таймер сценария отменён','manual','info'],output_unavailable:[target(l),`Недоступно для ${by}`,'device','bad'],timeout:[target(l),'Не ответило за 5 секунд','device','bad'],failed:[target(l),`Команда ${by} не выполнена`,'rule','bad'],pending_capacity:['Хаб','Очередь сценариев заполнена','device','bad']};
  const[title,sub,source,tone]=map[l.reason]||[target(l),l.reason,'device','muted'];rows.push({ms:l.monotonic_ms,title,sub,source,tone});}
 return rows.reverse();}
const sourceIcon={rule:'bolt',manual:'hand',device:'signal'};const sourceName={rule:'сценарий',manual:'вручную',device:'устройство'};
function dayLabel(d){const today=new Date();const y=new Date(today);y.setDate(today.getDate()-1);const same=(a,b)=>a.toDateString()===b.toDateString();if(same(d,today))return 'Сегодня';if(same(d,y))return 'Вчера';return d.toLocaleDateString('ru-RU',{day:'numeric',month:'long'});}
export function renderJournal(root,entries,rules,devices,hubNowMs){root.replaceChildren();const rows=buildJournal(entries,rules,devices);
 if(!rows.length){const empty=el('div',undefined,{class:'empty-state'});empty.append(el('h2','Пока тихо'),el('p','Здесь появятся срабатывания сценариев и команды устройствам.',{class:'muted'}));root.append(empty);return;}
 const now=Date.now();let day='',list=null;
 for(const r of rows){const when=new Date(now-Math.max(0,(hubNowMs??r.ms)-r.ms));const label=dayLabel(when);
  if(label!==day){day=label;root.append(el('h2',label,{class:'day-label'}));list=el('ol',undefined,{class:'timeline'});root.append(list);}
  const item=el('li',undefined,{class:`event tone-${r.tone}`});const dot=el('span',undefined,{class:`event-dot src-${r.source}`,title:sourceName[r.source]});dot.append(icon(sourceIcon[r.source]));
  const body=el('div',undefined,{class:'event-body'});body.append(el('span',r.title,{class:'event-title'}),el('span',r.sub,{class:'event-sub'}));
  item.append(el('time',when.toLocaleTimeString('ru-RU',{hour:'2-digit',minute:'2-digit'}),{datetime:when.toISOString()}),dot,body);list.append(item);}}
