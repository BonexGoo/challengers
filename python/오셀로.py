import challengers
import threading

server = challengers.Server(8877)
othello = server.app("오셀로", 200, 200)

tiles = [
    [0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 1, 2, 0, 0, 0],
    [0, 0, 0, 2, 1, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0],
    [0, 0, 0, 0, 0, 0, 0, 0]]
userturn = True

def TestScore(x, y, v):
    sumtiles = []
    if tiles[y][x] == 0:
        for i in range(-1, 2):
            for j in range(-1, 2):
                if i == 0 and j == 0:
                    continue
                collect = []
                x2 = x + i
                y2 = y + j
                while 0 <= x2 and x2 < 8 and 0 <= y2 and y2 < 8:
                    if tiles[y2][x2] == 3 - v:
                        collect.append([x2, y2])
                    else:
                        if tiles[y2][x2] == v:
                            sumtiles.extend(collect)
                        break
                    x2 += i
                    y2 += j
        if 0 < len(sumtiles):
            sumtiles.append([x, y])
    return sumtiles

def AddTile(sumtiles, v):
    for curtile in sumtiles:
        tiles[curtile[1]][curtile[0]] = v

def PrintTile():
    black = 0
    white = 0
    for y in range(8):
        for x in range(8):
            othello.set(f"d.tile_{x}_{y}", tiles[y][x])
            if tiles[y][x] == 1:
                black += 1
            elif tiles[y][x] == 2:
                white += 1
    othello.set("d.result", f"'BLACK <{black} : {white}> WHITE'")

def OnAiTurn():
    besttiles = []
    for y in range(8):
        for x in range(8):
            sumtiles = TestScore(x, y, 2)
            if len(besttiles) < len(sumtiles):
                besttiles = sumtiles
    if 0 < len(besttiles):
        AddTile(besttiles, 2)
        PrintTile()
    userturn == True

def OnClick(idx):
    if userturn == False:
        return
    x = int(idx) % 8
    y = int(idx) // 8
    sumtiles = TestScore(x, y, 1)
    if 0 < len(sumtiles):
        AddTile(sumtiles, 1)
        PrintTile()
    userturn == False
    timer = threading.Timer(1.0, OnAiTurn)
    timer.start()

for y in range(8):
    for x in range(8):
        othello.oncall(f"click_{x}_{y}", OnClick, x + 8 * y)

PrintTile()
server.start()
print("프로그램이 종료되었습니다")
