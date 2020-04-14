import Gui

let panel = Panel("First Window")
let button = Button("Click me")
let text = Text("Some Text")
panel.append(subview: button)
panel.append(subview: text)

var onOffSwitch = false

while true {
  if button.didClick {
    button.size = Gui.Size(100, 28)
    if !onOffSwitch {
      text.text = "You clicked me!"
      onOffSwitch = true
    } else {
      text.text = "You unclicked me!"
      onOffSwitch = false
    }
  }
  Gui.Do()
}
