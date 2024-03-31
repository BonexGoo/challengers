import challengers
import random

server = challengers.Server(8877)
apps = [server.app("두더지", 100, 100),
    server.app("두더지", 450, 100),
    server.app("두더지", 800, 100),
    server.app("두더지", 100, 450),
    server.app("두더지", 450, 450),
    server.app("두더지", 800, 450),
    server.app("두더지", 100, 800),
    server.app("두더지", 450, 800),
    server.app("두더지", 800, 800)]

for app in apps:
    app.set("d.show", random.randint(0, 1))

def Punch(pos):
    for i in range(len(apps)):
        if i == pos:
            apps[i].set("d.show", 0)
        else:
            value = random.randint(0, 1)
            if value == 1:
                apps[i].set("d.show", 1)
                apps[i].call("페이드인")
            else:
                apps[i].set("d.show", 0)

def OnClick(pos):
    show = int(apps[pos].get("d.show"))
    if show == 1:
        Punch(pos)
        print("Yes!!!")
    else:
        print("No...")

for i in range(len(apps)):
    apps[i].oncall("클릭", OnClick, i)

server.start()
print("프로그램이 종료되었습니다")
