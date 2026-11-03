"""Small original fixtures for the production archive/cover reader."""
from pathlib import Path
import struct,zlib,zipfile,subprocess
root=Path('build/asset-fixtures');root.mkdir(parents=True,exist_ok=True)
def chunk(t,d):return struct.pack('>I',len(d))+t+d+struct.pack('>I',zlib.crc32(t+d))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',3,4,8,2,0,0,0))+chunk(b'IDAT',zlib.compress((b'\0'+b'\x90\x40\x60'*3)*4))+chunk(b'IEND',b'')
container='<container><rootfiles><rootfile full-path="OPS/book.opf"/></rootfiles></container>'
cases={
 'epub2': '<metadata><meta name="cover" content="art"/></metadata><manifest><item id="art" href="Images/art%20one.png"/></manifest>',
 'epub3': '<manifest><item properties="nav&#10;cover-image" href="Images/art%20one.png"/></manifest>',
 'guide': '<guide><reference type="cover" href="Text/front.xhtml#top"/></guide>',
 'spine': '<manifest><item id="front" href="Text/front.xhtml"/></manifest><spine><itemref idref="front"/></spine>',
 'unsafe': '<manifest><item properties="cover-image" href="../../outside.png"/></manifest>',
}
for name,body in cases.items():
 with zipfile.ZipFile(root/(name+'.epub'),'w') as z:
  z.writestr('META-INF/container.xml',container)
  z.writestr('OPS/book.opf','<package>'+body+'</package>')
  z.writestr('OPS/Images/art one.png',png)
  z.writestr('OPS/Text/front.xhtml','<!DOCTYPE html><html xmlns:xlink="http://www.w3.org/1999/xlink"><body><svg><image xlink:href="../Images/art%20one.png"/></svg></body></html>')
with zipfile.ZipFile(root/'comic.cbz','w') as z:
 for name in ['10.png','2.png','1.png','__MACOSX/._1.png']:z.writestr(name,png)
files=root/'pages';files.mkdir(exist_ok=True)
for name in ['10.png','2.png','1.png']:(files/name).write_bytes(png)
rar=Path('C:/Program Files/WinRAR/Rar.exe')
if rar.exists():
 for ver in ['5']:
  subprocess.run([str(rar),'a','-idq','-ep','-ma'+ver,str((root/('comic'+ver+'.cbr')).resolve()),str(files/'*.png')],check=True)
print('Archive fixtures generated')

