"""Small synthetic books for local renderer tests; no external ebook downloads."""
from pathlib import Path
import zipfile
root=Path('build/fixtures');root.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(root/'reader.epub','w') as z:
    z.writestr('mimetype','application/epub+zip',compress_type=zipfile.ZIP_STORED)
    z.writestr('META-INF/container.xml','<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container" version="1.0"><rootfiles><rootfile full-path="book.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
    z.writestr('book.opf','''<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="id" version="2.0"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Reading Test</dc:title><dc:identifier id="id">reader-test</dc:identifier><dc:language>en</dc:language></metadata><manifest><item id="one" href="one.xhtml" media-type="application/xhtml+xml"/><item id="two" href="two.xhtml" media-type="application/xhtml+xml"/><item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/></manifest><spine toc="ncx"><itemref idref="one"/><itemref idref="two"/></spine></package>''')
    z.writestr('toc.ncx','''<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1"><head/><docTitle><text>Reading Test</text></docTitle><navMap><navPoint id="a" playOrder="1"><navLabel><text>Chapter One</text></navLabel><content src="one.xhtml"/></navPoint><navPoint id="b" playOrder="2"><navLabel><text>Chapter Two</text></navLabel><content src="two.xhtml#start"/></navPoint></navMap></ncx>''')
    paragraph='The reader remembers the current page and keeps navigation available while the next page is prepared. Text size, margins, and line spacing can be adjusted independently. <em>Italic text</em> and <strong>bold text</strong> are included in this document to check font selection.'
    for chapter,name in [(1,'one.xhtml'),(2,'two.xhtml')]:
        z.writestr(name,'<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter</title></head><body><h1 id="start">Chapter '+str(chapter)+'</h1>'+''.join('<p>'+paragraph+'</p>' for _ in range(30))+'</body></html>')
objects=[b'<< /Type /Catalog /Pages 2 0 R >>',b'<< /Type /Pages /Kids [3 0 R] /Count 1 >>',b'<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 800] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>',b'<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>']
stream=b'BT /F1 28 Tf 60 700 Td (PDF reading test) Tj 0 -45 Td /F1 18 Tf (Original layout, zoom and pan.) Tj ET'
objects.append(b'<< /Length '+str(len(stream)).encode()+b' >>\nstream\n'+stream+b'\nendstream')
data=b'%PDF-1.4\n';offsets=[0]
for i,obj in enumerate(objects,1):offsets.append(len(data));data+=str(i).encode()+b' 0 obj\n'+obj+b'\nendobj\n'
xref=len(data);data+=b'xref\n0 6\n0000000000 65535 f \n'+b''.join(f'{p:010} 00000 n \n'.encode() for p in offsets[1:])+b'trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n'+str(xref).encode()+b'\n%%EOF\n'
(root/'reader.pdf').write_bytes(data)
print('Generated EPUB and PDF fixtures')
