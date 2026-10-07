"""Exercise the extracted user archive, not the checkout or developer Python."""
import json, os, subprocess, sys, tempfile, time, urllib.request, zipfile
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'host_sim'))
from support import rule

def session(command, env, first, virtual=True):
    options=['--no-browser','--port','0']+(['--virtual-time'] if virtual else [])
    p=subprocess.Popen(command+options,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8',env=env)
    try:
        line=p.stdout.readline()
        if not line:raise AssertionError(p.stderr.read())
        info=json.loads(line);url=info['url']
        def call(path,body=None,method=None):
            payload=None if body is None else json.dumps(body).encode('utf-8')
            request=urllib.request.Request(url+path,data=payload,method=method,headers={'Content-Type':'application/json'})
            with urllib.request.urlopen(request,timeout=10) as r:return r.status,json.loads(r.read())
        for name in ['', 'app.js','api.js','scenario_editor.js','devices_view.js','journal_view.js','simulator_panel.js','style.css']:
            with urllib.request.urlopen(url+'/'+name,timeout=10) as r:
                expected='text/javascript' if name.endswith('.js') else 'text/css' if name.endswith('.css') else 'text/html'
                assert r.headers.get_content_type()==expected
                assert r.status==200 and len(r.read())>0
        assert call('/api/v1/system')[1]['mode']=='simulator'
        if first:
            value=rule()
            if not virtual:value['actions'][1]['delay_ms']=500
            assert call('/api/v1/scenarios',value)[0]==201
            value['name']='Проверка сохранения';assert call('/api/v1/scenarios/1',value,'PUT')[0]==200
            value['name']='Свет на Windows';assert call('/api/v1/scenarios/1',value,'PUT')[0]==200
            assert call('/api/v1/devices/0000000000000001',{'name':'Дверь Дмитрия'},'PUT')[0]==200
            assert call('/api/v1/devices/0000000000000001',{'name':'Вход'},'PUT')[0]==200
            assert call('/api/v1/settings',{'offset_minutes':420},'PUT')[0]==200
            assert call('/api/v1/settings',{'offset_minutes':60},'PUT')[0]==200
        else:
            assert call('/api/v1/scenarios')[1]['scenarios'][0]['name']=='Свет на Windows'
            devices=call('/api/v1/devices')[1]['devices']
            assert devices[0]['name']=='Вход' and devices[1]['states']['occupancy'] is None
            assert call('/api/v1/system')[1]['offset_minutes']==60
        call('/api/v1/sim/report',{'device_id':'0000000000000002','capability':'occupancy','value':False})
        call('/api/v1/sim/report',{'device_id':'0000000000000003','capability':'contact','channel_id':1,'value':False})
        call('/api/v1/sim/report',{'device_id':'0000000000000002','capability':'occupancy','value':True})
        assert call('/api/v1/devices')[1]['devices'][2]['channels'][0]['power'] is True
        if virtual:call('/api/v1/sim/advance',{'advance_ms':60000})
        else:time.sleep(1.2)
        assert call('/api/v1/devices')[1]['devices'][2]['channels'][0]['power'] is False
    finally:
        p.terminate();p.wait(timeout=10);p.stdout.close();p.stderr.close()
        # Closing the parent closes the child's stdin; let it release its files.
        time.sleep(.3)

if __name__=='__main__':
    with tempfile.TemporaryDirectory() as temp:
        root=Path(temp)/'Дмитрий и датчики';root.mkdir()
        with zipfile.ZipFile(sys.argv[1]) as z:z.extractall(root)
        env=dict(os.environ,LOCALAPPDATA=str(root/'Профиль пользователя'))
        executable=root/'ZigbeeRubi'/'ZigbeeRubi.exe'
        session([str(executable)],env,True);session([str(executable)],env,False)
        real_env=dict(env,LOCALAPPDATA=str(root/'Профиль с реальным временем'))
        session([str(executable)],real_env,True,virtual=False)
        assert (root/'Профиль пользователя/ZigbeeRubi/Simulator/devices0.bin').is_file()
        print('Packaged Windows ZIP: UI assets, C++ engine, timer, Unicode paths, replacement writes and restart PASS')
