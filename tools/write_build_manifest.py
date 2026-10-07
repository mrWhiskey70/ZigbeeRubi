"""Record actual built artifact checksums; never claim hardware validation."""
import hashlib,json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
build=root/(sys.argv[1] if len(sys.argv)>1 else 'build-c6')
config=(root/'sdkconfig').read_text()
assert 'CONFIG_IDF_TARGET="esp32c6"' in config and 'CONFIG_ESPTOOLPY_FLASHSIZE="16MB"' in config
files=['zigbee_gateway.bin','bootloader/bootloader.bin','partition_table/partition-table.bin','ota_data_initial.bin']
manifest={'schema':1,'hardware_verified':False,'idf_version':'v5.5.2','target':'esp32c6','flash_mb':16,'ota_slot_bytes':0x300000,'nvs_bytes':0x80000,'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'source_dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=root,text=True)),'upstream_commit':'1838e5b35bfaf3cc8b328ddc044b997019533881','dependencies':{'esp-zigbee-lib':'1.6.8','esp-zboss-lib':'1.6.4','mdns':'1.9.1'},'artifacts':{}}
source_paths=sorted([p for folder in ['components','main','cmake'] for p in (root/folder).rglob('*') if p.is_file()]+[root/'CMakeLists.txt',root/'sdkconfig.defaults',root/'sdkconfig.defaults.esp32c6',root/'partitions.csv',root/'dependencies.lock'])
source_hash=hashlib.sha256()
for p in source_paths:
 source_hash.update(str(p.relative_to(root)).encode()+b'\0'+p.read_bytes()+b'\0')
manifest['source_content_sha256']=source_hash.hexdigest()
for name in files:
 data=(build/name).read_bytes();manifest['artifacts'][name]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
assert manifest['artifacts']['zigbee_gateway.bin']['bytes']<manifest['ota_slot_bytes']
(root/'docs/build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Build manifest written; hardware_verified=false')
