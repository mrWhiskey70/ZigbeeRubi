"""Portable desktop entry point; the simulator engine remains the C++ child."""
import os
import sys
from pathlib import Path
import server

def main():
    if sys.stdout is not None and hasattr(sys.stdout, 'reconfigure'):
        sys.stdout.reconfigure(encoding='utf-8')
    frozen = getattr(sys, 'frozen', False)
    resources = Path(sys._MEIPASS) if frozen else server.ROOT
    binary = resources/'_bin/zigbee_hub_sim.exe' if frozen else Path(os.environ.get('ZIGBEERUBI_SIM_BINARY',str(server.ROOT/'build-simulator/Release/zigbee_hub_sim.exe')))
    assets = resources/'assets' if frozen else server.ROOT/'components/web_ui/assets'
    data = Path(os.environ.get('LOCALAPPDATA', str(Path.home()/'.local/share')))/'ZigbeeRubi'/'Simulator'
    options = list(sys.argv[1:])
    open_browser = '--no-browser' not in options
    if not open_browser:options.remove('--no-browser')
    argv = ['--binary',str(binary),'--assets',str(assets),'--data-dir',str(data)]+options
    def announce(info):
        print('ZigbeeRubi — симулятор запущен.',flush=True)
        print('Интерфейс: '+info['url'],flush=True)
        print('Оставьте это окно открытым. Для остановки закройте его или нажмите Ctrl+C.',flush=True)
    try:
        if open_browser:argv.append('--open')
        server.main(argv,on_ready=announce if open_browser else None)
    except KeyboardInterrupt:
        pass
    except Exception as error:
        print('Не удалось запустить симулятор: '+str(error),flush=True)
        if open_browser and sys.stdin is not None and sys.stdin.isatty():
            input('Нажмите Enter, чтобы закрыть окно.')
        return 1
    return 0

if __name__ == '__main__':sys.exit(main())
