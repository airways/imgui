
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

public class ViewContainer {
  private var subviews: [View] = []
  public func add(subview: View) {
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
  static private var panels: [View] = []
  static func add(panel: View) {
    panels.append(panel)
  }
  static func remove(panel: View) {
    panels.removeAll(where: { $0.viewID == panel.viewID })
  }
  static func renderAll() {
    for panel in panels {
      panel.render()
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

public class Panel: ViewContainer, View {
  public let viewID: Int = obtainViewID()
  public var title: String
  public init(_ title: String) {
    self.title = title
    super.init()
    Viewport.add(panel: self)
  }

  public func destroy() {
    Viewport.remove(panel: self)
  }

  public func render() {
    igPushIDInt(Int32(viewID))
    igBegin(title, nil, 0)
    renderSubviews()
    igEnd()
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
