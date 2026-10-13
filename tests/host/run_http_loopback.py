#!/usr/bin/env python3
"""Optional Linux test with installed FFmpeg shared libraries (no network download).
Only loopback HTTP is used. ABI declarations use opaque pointers from the test
fixture; the native build continues to use its SDK's real FFmpeg headers.
"""
import ctypes.util
import http.server
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading
import time
from urllib.parse import urlsplit
root=Path(__file__).resolve().parents[2]
fmt=ctypes.util.find_library('avformat');util=ctypes.util.find_library('avutil')
if not fmt or not util:
    print('SKIP: Optional real-FFmpeg loopback test needs host libavformat/libavutil.');raise SystemExit(0)
cxx=os.environ.get('CXX_HOST') or shutil.which('clang++') or shutil.which('g++')
cc=os.environ.get('CC_HOST') or shutil.which('clang') or shutil.which('gcc')
if not cxx or not cc:raise SystemExit('No host C/C++ compiler')
class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version='HTTP/1.1'
    played=False;favorite=False;mutations=0
    def answer(self,status,data=b'',length=True,extra=0,chunked=False):
        self.send_response(status);self.send_header('Content-Type','application/json')
        self.send_header('Connection','close')
        if length:self.send_header('Content-Length',str(len(data)+extra))
        if chunked:self.send_header('Transfer-Encoding','chunked')
        self.end_headers()
        try:
            self.wfile.write((f'{len(data):x}\r\n'.encode()+data+b'\r\n0\r\n\r\n') if chunked else data)
        except (BrokenPipeError,ConnectionResetError):pass
        self.close_connection=True
    def do_GET(self):
        path=urlsplit(self.path).path;data=b'{"Played":true}'
        if path=='/slow':
            self.send_response(200);self.send_header('Content-Length','1000');self.end_headers()
            try:
                for _ in range(20):self.wfile.write(b'x');self.wfile.flush();time.sleep(.15)
            except (BrokenPipeError,ConnectionResetError):pass
            self.close_connection=True;return
        if path=='/denied':self.answer(403);return
        if path in ('/empty','/no-content'):self.answer(204 if path=='/no-content' else 200);return
        if path=='/truncated':self.answer(200,data[:-5],extra=25);return
        if path=='/badlencomplete':self.answer(200,data,extra=25);return
        if path=='/m3u':self.answer(200,b'#EXTM3U\n#EXTINF:-1,Test\nhttp://example.test/live.ts\n',length=False);return
        if path=='/emby/Users/u/Items/film':
            data=json.dumps({'Id':'film','Type':'Movie','UserData':{'Played':Handler.played,'IsFavorite':Handler.favorite}}).encode()
            self.answer(200,data,length=False);return
        self.answer(200,data,length=path not in ('/noclength','/chunked'),chunked=path=='/chunked')
    def mutate(self,on):
        path=urlsplit(self.path).path
        if path.endswith('/PlayedItems/film'):Handler.played=on
        elif path.endswith('/FavoriteItems/film'):Handler.favorite=on
        else:self.answer(404);return
        Handler.mutations+=1
        if Handler.mutations%3==0:self.answer(204);return
        data=json.dumps({'ItemId':'film','Played':Handler.played,'IsFavorite':Handler.favorite}).encode()
        self.answer(200,data,length=False)
    def do_POST(self):self.mutate(True)
    def do_DELETE(self):self.mutate(False)
    def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',0),Handler)
thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
try:
    with tempfile.TemporaryDirectory(prefix='emby5-http-loopback-') as work:
        obj=Path(work)/'cjson.o';binary=Path(work)/'http-loopback'
        subprocess.run([cc,'-std=c11','-I'+str(root/'app/engine/addons/include'),'-c',str(root/'app/engine/addons/src/cJSON.c'),'-o',str(obj)],check=True)
        subprocess.run([cxx,'-std=c++17','-pthread','-I'+str(root/'tests/host/transport-stubs'),'-I'+str(root/'app/src'),'-I'+str(root/'app/engine/addons/include'),str(root/'tests/host/http_loopback_test.cpp'),str(root/'app/src/jf/jf_client.cpp'),str(root/'app/src/app/user_data_update.cpp'),str(root/'app/src/jf/jf_http_avio.cpp'),str(obj),'-l:'+fmt,'-l:'+util,'-o',str(binary)],check=True)
        subprocess.run([str(binary),'http://127.0.0.1:'+str(server.server_port)],check=True,timeout=45)
        assert Handler.mutations==80, 'A mutation was retried or omitted'
finally:
    server.shutdown();server.server_close();thread.join()
