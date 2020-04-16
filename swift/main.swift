import Gui

let panel = Panel(title: "First Window")
let button = Button(title: "Click me")
let text = Text(text: "Some Text")
panel.add(subview: button)
panel.add(subview: text)
let childPanel = Panel(title: "Child Panel")
let childText = TextInput(title: "Text")
childText.multiline = true
childPanel.add(subview: childText)
panel.add(subview: childPanel)

// This is the event-driven style.
button.onClick = {
  let panel = Panel(title: "Second Window")
  let text = Text(text: "Some Text")
  panel.add(subview: text)
  text.text = childText.text
  childText.onTextChange = {
    text.text = childText.text
  }
}

/*
// This is the polling style.
var onOffSwitch = false

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
