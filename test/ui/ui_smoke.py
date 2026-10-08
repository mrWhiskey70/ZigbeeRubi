"""Browser-to-API scenario test, including mobile layout and screenshots."""
import json,sys,tempfile,time,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'host_sim'))
from support import Server
from playwright.sync_api import sync_playwright
ROOT=Path(__file__).resolve().parents[2]
class Ui(unittest.TestCase):
 def test_full_path(self):
  s=Server()
  try:
   with sync_playwright() as p:
    browser=p.chromium.launch(headless=True,args=['--no-sandbox'])
    page=browser.new_page(viewport={'width':1366,'height':768});errors=[];page.on('pageerror',lambda e:errors.append(str(e)))
    page.goto(s.url);page.locator('#devices .device-card').first.wait_for()
    tab=lambda name:page.locator(f'[data-tab="{name}"]').click()
    page.locator('[data-tab="scenarios"]').click();page.get_by_role('button',name='+ Создать сценарий',exact=True).click()
    form=page.locator('#editor-dialog');form.locator('.lv-idea',has_text='Свет при движении').click()
    form.locator('.lv-add',has_text='Действия').click();form.locator('.lv-idea',has_text='Канал 1').click();form.get_by_role('button',name='Готово',exact=True).click()
    self.assertEqual(page.locator('input[name="name"]').input_value(),'Свет при движении');page.get_by_role('button',name='Сохранить сценарий',exact=True).click()
    page.locator('#editor-dialog').wait_for(state='hidden');self.assertEqual(len(s.call('/api/v1/scenarios')[1]['scenarios']),1)
    tab('settings');page.get_by_role('button',name='Обнаружить',exact=True).click()
    tab('devices');page.get_by_role('button',name='Канал 1 — Включён',exact=True).wait_for()
    tab('settings');page.get_by_role('button',name='+ 59 с',exact=True).click();tab('devices');page.get_by_role('button',name='Канал 1 — Включён',exact=True).wait_for()
    tab('settings');page.get_by_role('button',name='+ 1 с',exact=True).click();tab('devices');page.get_by_role('button',name='Канал 1 — Выключен',exact=True).wait_for()
    evidence=ROOT/'docs/evidence';evidence.mkdir(exist_ok=True)
    page.evaluate('window.scrollTo(0,0)');page.screenshot(path=str(evidence/'simulator-desktop.png'),full_page=True)
    page.set_viewport_size({'width':390,'height':844});page.evaluate('window.scrollTo(0,0)');page.screenshot(path=str(evidence/'simulator-mobile.png'),full_page=True)
    self.assertFalse(page.evaluate('document.documentElement.scrollWidth > window.innerWidth'))
    tab('settings');page.get_by_role('button',name='Сбросить',exact=True).click();page.get_by_role('button',name='Обнаружить',exact=True).click();tab('devices');page.get_by_role('button',name='Канал 1 — Включён',exact=True).wait_for();page.close()
    s.call('/api/v1/sim/advance',{'advance_ms':60000});self.assertIs(s.call('/api/v1/devices')[1]['devices'][2]['channels'][0]['power'],False)
    page=browser.new_page();page.goto(s.url);page.locator('[data-tab="journal"]').click();page.locator('.event').filter(has_text='Включён сценарием').first.wait_for()
    self.assertEqual(errors,[]);browser.close()
  finally:s.close()
if __name__=='__main__':unittest.main()
