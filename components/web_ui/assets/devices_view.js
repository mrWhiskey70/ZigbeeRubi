import {el,button,icon} from './api.js';
// Плитки: датчик — состояние крупно, нажатие открывает карточку устройства;
// исполнительное устройство — строки каналов, вся строка переключает канал.
export function renderDevices(root,devices,{command,open,join,pending={}}){root.replaceChildren();
 if(!devices.length){const empty=el('div',undefined,{class:'empty-state wide'});const mark=el('div',undefined,{class:'empty-icon'});mark.append(icon('plus'));empty.append(mark,el('h2','Подключите первое устройство'),el('p','Хаб откроет сеть на 60 секунд — переведите датчик в режим сопряжения.',{class:'muted'}),button('Добавить устройство',join,'primary'));root.append(empty);return;}
 // Комнаты и избранное появляются, только когда в данных устройства есть room/favorite
 // и устройств больше шести: до этого заголовки секций — лишний шум.
 const grouped=devices.length>6&&devices.some(d=>d.room||d.favorite);
 if(grouped){const order=[];const groups=new Map();const put=(k,d)=>{if(!groups.has(k)){groups.set(k,[]);order.push(k);}groups.get(k).push(d);};
  for(const d of devices)if(d.favorite)put('Избранное',d);for(const d of devices)put(d.room||'Без комнаты',d);
  const self=renderDevices;for(const k of order){root.append(el('h2',k,{class:'room-label'}));const box=el('div',undefined,{class:'device-grid nested'});root.append(box);self(box,groups.get(k).map(d=>({...d,favorite:false,room:''})),{command,open,join,pending});}return;}
 for(const d of devices){const actuator=d.channels.length>0;const card=el('article',undefined,{class:actuator?'device-card actuator':'device-card'});card.dataset.deviceId=d.device_id;
  const kind=actuator?'switch':d.capabilities.includes('contact')?'door':'motion';
  if(!actuator){const contact=kind==='door',value=d.states[contact?'contact':'occupancy'];const known=d.available&&value!==null&&value!==undefined;const active=known&&value===true;
   const tile=el('button',undefined,{type:'button',class:'tile-open','aria-label':`${d.name}: ${known?(contact?(value?'открыта':'закрыта'):(value?'есть движение':'нет движения')):'нет данных'}. Открыть карточку`});
   const mark=el('span',undefined,{class:`device-icon ${active?'active':''} ${known?'':'off'}`});mark.append(icon(kind));
   tile.append(mark,el('span',d.name,{class:'tile-name'}),el('span',known?(contact?(value?'Открыта':'Закрыта'):(value?'Есть движение':'Нет движения')):(d.available?'Нет данных':'Не отвечает'),{class:`tile-state ${known?'':'unknown'}`}));
   tile.addEventListener('click',()=>open(d));card.append(tile);root.append(card);continue;}
  const head=el('button',undefined,{type:'button',class:'card-head','aria-label':`${d.name}. Открыть карточку`});const mark=el('span',undefined,{class:`device-icon ${d.available?'':'off'}`});mark.append(icon(kind));const title=el('span',undefined,{class:'card-title'});title.append(el('span',d.name,{class:'tile-name'}),el('small',d.available?`${d.channels.filter(c=>c.power===true).length} из ${d.channels.length} включено`:'Не отвечает'));const more=el('span',undefined,{class:'card-menu'});more.append(icon('more'));head.append(mark,title,more);head.addEventListener('click',()=>open(d));card.append(head);
  for(const c of d.channels){const key=`${d.device_id}|${c.channel_id}`;const state=pending[key]?'Команда…':!d.available||c.power===null?'Неизвестно':c.power?'Включён':'Выключен';
   const row=el('div',undefined,{class:'channel-row'});const label=el('span',`Канал ${c.channel_id}`);label.append(el('small',state));
   const toggle=button('',()=>command(d,c),`switch ${c.power===null?'unknown':c.power?'on':''}`);toggle.setAttribute('aria-label',`Канал ${c.channel_id} — ${state}`);toggle.setAttribute('aria-pressed',String(c.power===true));toggle.disabled=!!pending[key]||!d.available||!c.supported;
   row.append(label,toggle);card.append(row);}
  root.append(card);}}
