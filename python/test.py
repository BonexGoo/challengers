import challengers

server = challengers.Server(8877)
app1 = server.app("todolist", 100, 100)
app2 = server.app("계산기", 1000, 100)

app1.set("d.list.0", "영어공부")
app1.set("d.list.1", "다이어트")
app1.set("d.list.2", "쇼핑")
app1.set("d.list.count", 3)

app2.set("d.list.0", "수학공부")
app2.set("d.list.1", "취침")
app2.set("d.list.count", 2)

def OnStart1():
    name = app1.get("d.data.widgets.1.name")
    print(f"AAAAAAAAAAAA! - {name}")
    app1.call("AAA")

app1.oncall(OnStart1, "Start")
server.start()
