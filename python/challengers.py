import subprocess
import websockets
import asyncio
import threading
import time

wspacket = dict()
wsfunc = dict()
wsput = dict()

def _call(appid, callname):
    if wsfunc.get(appid) != None and wsfunc[appid].get(callname) != None:
        wsfunc[appid][callname][0](wsfunc[appid][callname][1])

async def _accept(websocket):
    appid = -1
    try:
        while True:
            try:
                comma_params = await asyncio.wait_for(websocket.recv(), 0.05)
                #print(f"recv[app{appid}] → {comma_params}")
            except asyncio.TimeoutError:
                if appid != -1:
                    for curpacket in wspacket[appid]:
                        await websocket.send(curpacket)
                        #print(f"send[app{appid}] ← {curpacket}")
                    wspacket[appid] = []
            else:
                params = comma_params.split(',')
                if params[0] == "init":
                    appid = params[1]
                    print(f"App Connected - [app{appid}]")
                elif appid != -1:
                    if params[0] == "call":
                        threading.Thread(target=_call, args=(appid, params[1])).start()
                    elif params[0] == "put":
                        wsput[appid] = params[1]
    except (websockets.exceptions.ConnectionClosedOK, websockets.exceptions.ConnectionClosedError):
        print(f"App Exit - [app{appid}]")
        if appid != -1:
            del wspacket[appid]
            if len(wspacket) == 0:
                asyncio.get_event_loop().stop()

class App(object):
    def __init__(self, appname, appx, appy, appid, port):
        self._appid = str(appid)
        wspacket[self._appid] = []
        subprocess.Popen(["C:/AllProjects/BossProjects/challengers/bin_Release64/challengers.exe",
            appname, str(appx), str(appy), "100", self._appid, str(port)])
        print(f"App Start - [app{self._appid}:{appname}]")

    def set(self, key, value):
        if wspacket.get(self._appid) == None:
            return
        wspacket[self._appid].append(f"set,{key},{value}#")

    def get(self, key):
        if wspacket.get(self._appid) == None:
            return "<closed>"
        wspacket[self._appid].append(f"get,{key}#")
        count = 0
        while wsput.get(self._appid) == None:
            time.sleep(0.05)
            count += 1
            if count == 20:
                return "<timeout>"
        ret = wsput[self._appid]
        del wsput[self._appid]
        return ret

    def call(self, callname):
        if wspacket.get(self._appid) == None:
            return
        wspacket[self._appid].append(f"call,{callname}#")

    def oncall(self, callname, func, param):
        if wsfunc.get(self._appid) == None:
            wsfunc[self._appid] = dict()
        wsfunc[self._appid][callname] = [func, param]

class Server(object):
    def __init__(self, port):
        self._appidx = -1
        self._port = port
        self._server = websockets.serve(_accept, "127.0.0.1", self._port)
        print(f"Server Start - [{self._port}]")

    def app(self, appname, appx, appy):
        self._appidx += 1
        return App(appname, appx, appy, self._appidx, self._port)

    def start(self):
        asyncio.get_event_loop().run_until_complete(self._server)
        asyncio.get_event_loop().run_forever()
        print(f"Server Stop - [{self._port}]")
