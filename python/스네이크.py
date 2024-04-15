import challengers
import threading
import random

server = challengers.Server(8877)
snake = server.app("스네이크", 1600, 200)
width = 30
height = 22
snake.set("d.tilew", width)
snake.set("d.tileh", height)

way = 0
head = []
body = []
star = []
score = 0
tick = 0
live = 0

def PrintSnake():
    snake.set("d.head.x", head[0])
    snake.set("d.head.y", head[1])
    for i in range(len(body)):
        snake.set(f"d.body.{i}.x", body[i][0])
        snake.set(f"d.body.{i}.y", body[i][1])
    snake.set("d.body.count", len(body));
    snake.set("d.star.x", star[0])
    snake.set("d.star.y", star[1])
    snake.set("d.score", score)

def IsGameOver():
    if head[0] < 0 or width <= head[0] or head[1] < 0 or height <= head[1]:
        return True
    for i in range(len(body)):
        if head[0] == body[i][0] and head[1] == body[i][1]:
            return True
    return False

def RunOnce():
    global live
    if live == 0:
        return
    tailx = body[-1][0]
    taily = body[-1][1]
    for i in reversed(range(len(body) - 1)):
        body[i + 1][0] = body[i][0]
        body[i + 1][1] = body[i][1]
    body[0][0] = head[0]
    body[0][1] = head[1]
    if way == 0:
        head[1] -= 1
    elif way == 1:
        head[0] += 1
    elif way == 2:
        head[1] += 1
    elif way == 3:
        head[0] -= 1
    if IsGameOver():
        live = 0
    else:
        global star
        global score
        global tick
        if head[0] == star[0] and head[1] == star[1]:
            body.append([tailx, taily])
            star = [random.randint(0, width - 1), random.randint(0, height - 1)]
            score += 100
            tick *= 0.95
    PrintSnake()

def Reset():
    global way
    global head
    global body
    global star
    global score
    global tick
    global live
    way = 1
    head = [width // 2, height // 2]
    body = [[head[0] - 1, head[1]],
            [head[0] - 2, head[1]],
            [head[0] - 2, head[1] - 1],
            [head[0] - 2, head[1] - 2]]
    star = [random.randint(0, width - 1),
            random.randint(0, height - 1)]
    score = 0
    tick = 0.4
    live = 1

timer = None
def OnTimer():
    RunOnce()
    global timer
    if timer != None:
        timer.cancel()
    if live == 1:
        timer = threading.Timer(tick, OnTimer)
        timer.start()

def OnStart(nouse):
    Reset()
    OnTimer()

def OnLeft(nouse):
    global way
    way = (way + 3) % 4

def OnRight(nouse):
    global way
    way = (way + 1) % 4

snake.oncall("start", OnStart, 0)
snake.oncall("left", OnLeft, 0)
snake.oncall("right", OnRight, 0)
snake.oncall("KeyPress_Left", OnLeft, 0)
snake.oncall("KeyPress_Right", OnRight, 0)

Reset()
PrintSnake()
server.start()
print("프로그램이 종료되었습니다")
