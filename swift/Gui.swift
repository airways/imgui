
public protocol View {
  func render()
}

public class ViewContainer {
  private var subviews: [View] = []
  public func append(subview: View) {
    subviews.append(subview)
  }
  public func remove(at: Int) {
    subviews.remove(at: at)
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

public class Panel: ViewContainer, View {
  static private var panels: [Panel] = []
  static func renderAll() {
    for panel in panels {
      panel.render()
    }
  }

  var title: String
  public init(_ title: String) {
    self.title = title
    super.init()
    Panel.panels.append(self)
  }

  public func render() {
    igBegin(title, nil, 0)
    renderSubviews()
    igEnd()
  }
}

public class Button: View {
  public var title: String
  public var size: Size
  public private(set) var didClick: Bool = false
  public init(_ title: String, size: Size = Size(56, 28)) {
    self.title = title
    self.size = size
  }

  public func render() {
    didClick = igButton(title, ImVec2(x: size.x, y: size.y))
  }
}

public class Text: View {
  public var text: String
  public init(_ text: String) {
    self.text = text
  }

  public func render() {
    igTextUnformatted(text, nil)
  }
}

public func Do() {
  Panel.renderAll()
  macosResume()
}
