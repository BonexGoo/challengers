import subprocess
import websockets
import asyncio
import threading
import time

wspacket = dict()
wsfunc = dict()
wsput = dict()

def _send(appid, packet):
    if wspacket.get(appid) != None:
        wspacket[appid].append(packet)
    else:
        wspacket[appid] = [packet]

def _call(appid, callname):
    wsfunc[appid][callname]()

async def accept(websocket):
    appid = -1
    try:
        while True:
            try:
                comma_params = await asyncio.wait_for(websocket.recv(), 0.01)
                params = comma_params.split(',')
                if params[0] == "init":
                    appid = params[1]
                    print(f"App Connected! - [app{appid}]")
                elif params[0] == "call":
                    threading.Thread(target=_call, args=(appid, params[1])).start()
                elif params[0] == "put":
                    wsput[appid] = params[1]
                #print(f"recv[app{appid}] → {comma_params}")
            except(asyncio.TimeoutError, ConnectionRefusedError):
                if appid != -1 and wspacket.get(appid) != None:
                    for curpacket in wspacket[appid]:
                        await websocket.send(curpacket)
                        #print(f"send[app{appid}] ← {curpacket}")
                    del wspacket[appid]
    except(websockets.exceptions.ConnectionClosedOK, websockets.exceptions.ConnectionClosedError) as e:
        print(f"App Exit! - [app{appid}]")

class App(object):

    def __init__(self, appname, appx, appy, appid, port):
        self._appid = appid
        subprocess.Popen(["C:/AllProjects/BossProjects/challengers/bin_Release64/challengers.exe",
            appname, str(appx), str(appy), "100", str(self._appid), str(port)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        print(f"App Start! - [app{self._appid}:{appname}]")

    def set(self, key, value):
        appid = str(self._appid)
        _send(appid, f"set,{key},{value}#")

    def get(self, key):
        appid = str(self._appid)
        _send(appid, f"get,{key}#")
        count = 0
        while wsput.get(appid) == None:
            time.sleep(0.01)
            count += 1
            if count == 100:
                return "<timeout>"
        ret = wsput[appid]
        del wsput[appid]
        return ret

    def call(self, callname):
        appid = str(self._appid)
        _send(appid, f"call,{callname}#")

    def oncall(self, func, callname):
        appid = str(self._appid)
        if wsfunc.get(appid) == None:
            wsfunc[appid] = dict()
        wsfunc[appid][callname] = func

class Server(object):

    def __init__(self, port):
        self._appid = -1
        self._port = port
        self._server = websockets.serve(accept, "127.0.0.1", self._port)
        print(f"Server Start! - [{self._port}]")

    def app(self, appname, appx, appy):
        self._appid += 1
        return App(appname, appx, appy, self._appid, self._port)

    def start(self):
        asyncio.get_event_loop().run_until_complete(self._server)
        asyncio.get_event_loop().run_forever()
