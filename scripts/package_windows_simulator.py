"""Build a self-contained Windows archive; run on the Windows CI runner."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--binary',type=Path,default=ROOT/'build-simulator/Release/zigbee_hub_sim.exe')
    parser.add_argument('--output',type=Path,default=ROOT/'build-windows-release')
    args=parser.parse_args()
    binary=args.binary.resolve();output=args.output.resolve()
    if not binary.is_file():raise FileNotFoundError(binary)
    subprocess.run([sys.executable,'-m','PyInstaller','--noconfirm','--clean','--onedir','--console',
                    '--name','ZigbeeRubi','--distpath',str(output/'dist'),'--workpath',str(output/'work'),
                    '--specpath',str(output/'work'),'--paths',str(ROOT/'host'),
                    '--add-binary',str(binary)+':_bin','--add-data',str(ROOT/'components/web_ui/assets')+':assets',
                    str(ROOT/'host/windows_launcher.py')],check=True)
    bundle=output/'dist/ZigbeeRubi'
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    subprocess.run(['git','archive','HEAD','--prefix=ZigbeeRubi-source/','-o',str(bundle/'source.zip')],cwd=ROOT,check=True)
    shutil.copyfile(ROOT/'LICENSE',bundle/'LICENSE')
    shutil.copyfile(ROOT/'upstream-baseline.json',bundle/'upstream-baseline.json')
    (bundle/'START.txt').write_text('''ZigbeeRubi — симулятор для Windows 10/11 x64

1. Распакуйте ВЕСЬ архив в любую папку.
2. Дважды нажмите ZigbeeRubi.exe. Интерфейс откроется в браузере.
3. Оставьте окно приложения открытым. Закройте его для остановки.

Первый тест: Сценарии → Создать сценарий → Сохранить сценарий.
В панели симулятора нажмите Обнаружить. Канал 1 включится и через 60 секунд выключится.
После перезапуска нажмите Отчёты нормы, затем Обнаружить: первые отчёты восстанавливают состояния всех устройств.

Сохранённые данные: %LOCALAPPDATA%\\ZigbeeRubi\\Simulator.
Если порт 8080 занят, закройте предыдущий экземпляр приложения.
Не переносите EXE отдельно от папки _internal.

Исходники и лицензия AGPL-3.0-only включены в source.zip и LICENSE.
Актуальный проект: https://github.com/mrWhiskey70/ZigbeeRubi
''',encoding='utf-8-sig')
    files={}
    for p in sorted(bundle.rglob('*')):
        if p.is_file():files[p.relative_to(bundle).as_posix()]={'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
    (bundle/'manifest.json').write_text(json.dumps({'schema':1,'platform':'windows-x64','source_commit':commit,'hardware_verified':False,'files':files},indent=2)+'\n')
    archive=output/'ZigbeeRubi-Windows-x64.zip'
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
        for p in sorted(bundle.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(bundle.parent).as_posix())
    print(str(archive))

if __name__=='__main__':main()
