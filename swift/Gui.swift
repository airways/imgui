
public class View {
  static var globalID = 0
  let viewID: Int
  init() {
    View.globalID += 1
    self.viewID = View.globalID
  }
  func render() {
    fatalError("subclass needs to implement render()")
  }
}

public struct Size {
  public let width: Float
  public let height: Float
  public init(width: Float, height: Float) {
    self.width = width
    self.height = height
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

public class ContainerView: View {
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
  weak var parent: ContainerView?
}

public class Panel: ContainerView {
  public var title: String
  public var size: Size
  public init(title: String, size: Size = Size(width: 0, height: 0)) {
    self.title = title
    self.size = size
    super.init()
    Viewport.add(panel: self)
  }

  public func destroy() {
    if let parent = parent {
      parent.remove(subview: self)
    } else {
      Viewport.remove(panel: self)
    }
  }

  override func render() {
    igPushIDInt(Int32(viewID))
    if parent != nil {
      igBeginChildStr(title, ImVec2(x: size.width, y: size.height), false, 0)
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
  public var title: String
  public var size: Size
  public private(set) var didClick: Bool = false
  public var onClick: (() -> Void)?
  public init(title: String, size: Size = Size(width: 0, height: 0)) {
    self.title = title
    self.size = size
  }

  override func render() {
    igPushIDInt(Int32(viewID))
    didClick = igButton(title, ImVec2(x: size.width, y: size.height))
    igPopID()
    if didClick, let onClick = onClick {
      Viewport.add(event: onClick)
    }
  }
}

public class Text: View {
  public var text: String
  public init(text: String) {
    self.text = text
  }

  override func render() {
    igPushIDInt(Int32(viewID))
    igTextUnformatted(text, nil)
    igPopID()
  }
}

public class TextInput: View {
  public var title: String
  public var multiline: Bool = false
  public var size: Size
  public var text: String { String(cString: textDataStr(textData)) }
  public private(set) var didTextChange: Bool = false
  public var onTextChange: (() -> Void)?
  public init(title: String, size: Size = Size(width: 0, height: 0)) {
    self.title = title
    self.size = size
  }

  private var textData: UnsafeMutableRawPointer?
  deinit {
    guard let textData = textData else { return }
    deinitTextData(textData)
  }
  override func render() {
    igPushIDInt(Int32(viewID))
    if textData == nil {
      textData = initTextData()
    }
    if multiline {
      igtxInputTextMultiline(title, textData, ImVec2(x: size.width, y: size.height), 0)
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
  commonResume()
  ThisTick.rendered = false
}
