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


(root/'sample.fb2').write_text('<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"><description><title-info><book-title>Test book</book-title></title-info></description><body><section><title><p>Chapter</p></title><p>Original test text.</p></section></body></FictionBook>',encoding='utf-8')
(root/'sample.html').write_text('<!DOCTYPE html><html><head><title>Test</title></head><body><h1>Chapter</h1><p>Original test text.</p></body></html>',encoding='utf-8')
with zipfile.ZipFile(root/'sample.xps','w') as z:
 z.writestr('[Content_Types].xml','<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Override PartName="/FixedDocSeq.fdseq" ContentType="application/vnd.ms-package.xps-fixeddocumentsequence+xml"/></Types>')
 z.writestr('_rels/.rels','<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="r1" Type="http://schemas.microsoft.com/xps/2005/06/fixedrepresentation" Target="/FixedDocSeq.fdseq"/></Relationships>')
 z.writestr('FixedDocSeq.fdseq','<FixedDocumentSequence xmlns="http://schemas.microsoft.com/xps/2005/06"><DocumentReference Source="FixedDoc.fdoc"/></FixedDocumentSequence>')
 z.writestr('FixedDoc.fdoc','<FixedDocument xmlns="http://schemas.microsoft.com/xps/2005/06"><PageContent Source="Page.fpage"/></FixedDocument>')
 z.writestr('Page.fpage','<FixedPage xmlns="http://schemas.microsoft.com/xps/2005/06" Width="600" Height="800" xml:lang="en-US"><Path Fill="#FF884422" Data="M 50,50 L 550,50 550,750 50,750 Z"/></FixedPage>')
