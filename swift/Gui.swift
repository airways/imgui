
public protocol View {
  var viewID: Int { get }
  func render()
}

func obtainViewID() -> Int {
  struct ViewIDLot {
    static var viewID = 0
  }
  ViewIDLot.viewID += 1
  return ViewIDLot.viewID
}

public struct Size {
  public let x: Float
  public let y: Float
  public init(_ x: Float, _ y: Float) {
    self.x = x
    self.y = y
  }
}

class Viewport {
  static private var panels: [Panel] = []
  static func add(panel: Panel) {
    panels.append(panel)
  }
  static func remove(panel: Panel) {
    panels.removeAll(where: { $0.viewID == panel.viewID })
  }
  static func renderAll() {
    for panel in panels {
      if panel.parent == nil {
        panel.render()
      }
    }
  }

  static private var events: [() -> Void] = []
  static func add(event: @escaping () -> Void) {
    events.append(event)
  }
  static func drainEvents() {
    let events = self.events
    self.events = []
    for event in events {
      event()
    }
  }
}

public class Panel: View {

  private var subviews: [View] = []
  public func add(subview: View) {
    if let panel = subview as? Panel {
      panel.parent = self
    }
    subviews.append(subview)
  }
  public func remove(subview: View) {
    subviews.removeAll(where: { $0.viewID == subview.viewID })
  }
  func renderSubviews() {
    for subview in subviews {
      subview.render()
    }
  }

  public let viewID: Int = obtainViewID()
  public var title: String
  public var size: Size
  public init(_ title: String, size: Size = Size(0, 0)) {
    self.title = title
    self.size = size
    Viewport.add(panel: self)
  }

  weak var parent: Panel?

  public func destroy() {
    if let parent = parent {
      parent.remove(subview: self)
    } else {
      Viewport.remove(panel: self)
    }
  }

  public func render() {
    igPushIDInt(Int32(viewID))
    if parent != nil {
      igBeginChildStr(title, ImVec2(x: size.x, y: size.y), false, 0)
    } else {
      igBegin(title, nil, 0)
    }
    renderSubviews()
    if parent != nil {
      igEndChild()
    } else {
      igEnd()
    }
    igPopID()
  }
}

public class Button: View {
  public let viewID: Int = obtainViewID()
  public var title: String
  public var size: Size
  public private(set) var didClick: Bool = false
  public var onClick: (() -> Void)?
  public init(_ title: String, size: Size = Size(0, 0)) {
    self.title = title
    self.size = size
  }

  public func render() {
    igPushIDInt(Int32(viewID))
    didClick = igButton(title, ImVec2(x: size.x, y: size.y))
    igPopID()
    if didClick, let onClick = onClick {
      Viewport.add(event: onClick)
    }
  }
}

public class Text: View {
  public let viewID: Int = obtainViewID()
  public var text: String
  public init(_ text: String) {
    self.text = text
  }

  public func render() {
    igPushIDInt(Int32(viewID))
    igTextUnformatted(text, nil)
    igPopID()
  }
}

public class TextInput: View {
  public let viewID: Int = obtainViewID()
  public var title: String
  public var multiline: Bool = false
  public var size: Size
  public var text: String { String(cString: textDataStr(textData)) }
  public private(set) var didTextChange: Bool = false
  public var onTextChange: (() -> Void)?
  public init(_ title:String, size: Size = Size(0, 0)) {
    self.title = title
    self.size = size
  }

  private var textData: UnsafeMutableRawPointer?
  deinit {
    guard let textData = textData else { return }
    deinitTextData(textData)
  }
  public func render() {
    igPushIDInt(Int32(viewID))
    if textData == nil {
      textData = initTextData()
    }
    if multiline {
      igtxInputTextMultiline(title, textData, ImVec2(x: size.x, y: size.y), 0)
    } else {
      igtxInputText(title, textData, 0)
    }
    igPopID()
    didTextChange = textDataChanged(textData)
    if didTextChange, let onTextChange = onTextChange {
      Viewport.add(event: onTextChange)
    }
  }
}

@_cdecl("Gui_Do")
public func Do() {
  struct ThisTick {
    static var rendered = false
  }
  if !ThisTick.rendered {
    ThisTick.rendered = true
    Viewport.renderAll()
    Viewport.drainEvents()
  }
  macosResume()
  ThisTick.rendered = false
}
