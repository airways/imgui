import Gui

let panel = Panel("First Window")
let button = Button("Click me")
let text = Text("Some Text")
panel.add(subview: button)
panel.add(subview: text)

var onOffSwitch = false

button.onClick = {
  let panel = Panel("Second Window")
  let text = Text("Some Text")
  panel.add(subview: text)
  var counter = 0
  text.text = "Counter \(counter)"
  counter += 1
  Gui.Do()
}

/*
while true {
  if button.didClick {
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
*/
