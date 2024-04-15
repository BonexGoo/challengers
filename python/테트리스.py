import challengers
import threading
import random

server = challengers.Server(8877)
tetris = server.app("테트리스", 200, 200)

tiles = [
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0, 0]]
tilew = len(tiles[0])
tileh = len(tiles)
tetris.set("d.tilew", tilew)
tetris.set("d.tileh", tileh)
tetris.set("d.tiles", 40)

runstyles = [
    [[0, -1], [0, 0], [1, 0], [1, 1]],
    [[0, -1], [0, 0], [-1, 0], [-1, 1]],
    [[-1, 0], [0, 0], [1, 0], [0, 1]],
    [[0, 0], [1, 0], [0, 1], [1, 1]],
    [[0, -1], [0, 0], [0, 1], [1, 1]],
    [[0, -1], [0, 0], [0, 1], [-1, 1]],
    [[0, -2], [0, -1], [0, 0], [0, 1]]]
runtile = runstyles[0]
runpos = [0, 0]
nextstyle = random.randint(0, 6)

def CanMove(addx, addy):
    for i in range(4):
        x = runpos[0] + runtile[i][0] + addx
        y = runpos[1] + runtile[i][1] + addy
        if 0 <= x and x < tilew and 0 <= y and y < tileh:
            if tiles[y][x] != 0:
                return False
        elif x < 0 or tilew <= x or tileh <= y:
            return False
    return True

def CheckTile():
    for y in range(tileh):
        full = True
        for x in range(tilew):
            if tiles[y][x] == 0:
                full = False
                break
        if full:
            for y2 in reversed(range(y)):
                for x in range(tilew):
                    tiles[y2 + 1][x] = tiles[y2][x]
            for x in range(tilew):
                tiles[0][x] = 0

def NewRun():
    global runpos
    global runtile
    global nextstyle
    runtile = runstyles[nextstyle]
    runpos = [int(tilew / 2), 0]
    nextstyle = random.randint(0, 6)

def PasteTile():
    for i in range(4):
        x = runpos[0] + runtile[i][0]
        y = runpos[1] + runtile[i][1]
        if 0 <= x and x < tilew and 0 <= y and y < tileh:
            tiles[y][x] = 1
    CheckTile()
    NewRun()

def PrintTile():
    for y in range(tileh):
        for x in range(tilew):
            tetris.set(f"d.tile_{y}_{x}", tiles[y][x])
    for i in range(4):
        x = runpos[0] + runtile[i][0]
        y = runpos[1] + runtile[i][1]
        tetris.set(f"d.tile_{y}_{x}", 1)

def OnLeft(nouse):
    if CanMove(-1, 0):
        runpos[0] -= 1
        PrintTile()

def OnRight(nouse):
    if CanMove(1, 0):
        runpos[0] += 1
        PrintTile()

def OnDown(nouse):
    while CanMove(0, 1):
        runpos[1] += 1
    PrintTile()

def OnRotate(nouse):
    for i in range(4):
        x = runtile[i][0]
        y = runtile[i][1]
        runtile[i][0] = y
        runtile[i][1] = -x
    if CanMove(0, 0):
        PrintTile()
    else:
        for i in range(4):
            y = runtile[i][0]
            x = -runtile[i][1]
            runtile[i][0] = x
            runtile[i][1] = y

timer = None
def OnTimer():
    if CanMove(0, 1):
        runpos[1] += 1
    else:
        PasteTile()
    PrintTile()
    global timer
    if timer != None:
        timer.cancel()
    timer = threading.Timer(1.5, OnTimer)
    timer.start()

def OnStart(nouse):
    for y in range(tileh):
        for x in range(tilew):
            if y < tileh - 2:
                tiles[y][x] = 0
            else:
                tiles[y][x] = random.randint(0, 1)
    NewRun()
    OnTimer()

tetris.oncall("left", OnLeft, 0)
tetris.oncall("right", OnRight, 0)
tetris.oncall("down", OnDown, 0)
tetris.oncall("rotate", OnRotate, 0)
tetris.oncall("start", OnStart, 0)
tetris.oncall("KeyPress_Left", OnLeft, 0)
tetris.oncall("KeyPress_Right", OnRight, 0)
tetris.oncall("KeyPress_Down", OnDown, 0)
tetris.oncall("KeyPress_Up", OnRotate, 0)

server.start()
print("프로그램이 종료되었습니다")
